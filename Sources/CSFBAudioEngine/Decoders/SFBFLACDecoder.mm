//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBFLACDecoder.h"

#import "NSData+SFBExtensions.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>
#import <FLAC/metadata.h>
#import <FLAC/stream_decoder.h>

#import <os/log.h>

#import <algorithm>
#import <memory>

#import <simd/simd.h>

SFBAudioDecoderName const SFBAudioDecoderNameFLAC = @"org.sbooth.AudioEngine.Decoder.FLAC";
SFBAudioDecoderName const SFBAudioDecoderNameOggFLAC = @"org.sbooth.AudioEngine.Decoder.OggFLAC";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMinimumBlockSize = @"min_blocksize";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMaximumBlockSize = @"max_blocksize";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMinimumFrameSize = @"min_framesize";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMaximumFrameSize = @"max_framesize";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACSampleRate = @"sample_rate";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACChannels = @"channels";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACBitsPerSample = @"bits_per_sample";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACTotalSamples = @"total_samples";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMD5Sum = @"md5sum";

namespace {

/// A `std::unique_ptr` deleter for `FLAC__StreamDecoder` objects
struct flac__stream_decoder_deleter {
    void operator()(FLAC__StreamDecoder *decoder) {
        FLAC__stream_decoder_delete(decoder);
    }
};

using flac__stream_decoder_unique_ptr = std::unique_ptr<FLAC__StreamDecoder, flac__stream_decoder_deleter>;

} /* namespace */

@interface SFBFLACDecoder () {
  @private
    flac__stream_decoder_unique_ptr _flac;
    FLAC__StreamMetadata_StreamInfo _streamInfo;
    AVAudioFramePosition _framePosition;
    FLAC__FrameHeader _previousFrameHeader;
    AVAudioPCMBuffer *_frameBuffer; // For converting push to pull
    NSError *_writeError;
}
- (BOOL)initializeFLACStreamDecoder:(FLAC__StreamDecoder *)decoder error:(NSError **)error;
- (FLAC__StreamDecoderWriteStatus)handleFLACWrite:(const FLAC__StreamDecoder *)decoder
                                            frame:(const FLAC__Frame *)frame
                                           buffer:(const FLAC__int32 *const[])buffer;
- (void)handleFLACMetadata:(const FLAC__StreamDecoder *)decoder metadata:(const FLAC__StreamMetadata *)metadata;
- (void)handleFLACError:(const FLAC__StreamDecoder *)decoder status:(FLAC__StreamDecoderErrorStatus)status;
@end

#pragma mark FLAC Callbacks

namespace {

FLAC__StreamDecoderReadStatus readCallback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes,
                                           void *client_data) noexcept {
#pragma unused(decoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
    SFBInputSource *inputSource = flacDecoder->_inputSource;

    NSInteger bytesRead;
    if (![inputSource readBytes:buffer length:static_cast<NSInteger>(*bytes) bytesRead:&bytesRead error:nil]) {
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    *bytes = static_cast<size_t>(bytesRead);

    if (bytesRead == 0 && inputSource.atEOF) {
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderSeekStatus seekCallback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset,
                                           void *client_data) noexcept {
#pragma unused(decoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
    SFBInputSource *inputSource = flacDecoder->_inputSource;

    if (!inputSource.supportsSeeking) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
    }

    if (![inputSource seekToOffset:static_cast<NSInteger>(absolute_byte_offset) error:nil]) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }

    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus tellCallback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset,
                                           void *client_data) noexcept {
#pragma unused(decoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;

    NSInteger offset;
    if (![flacDecoder->_inputSource getOffset:&offset error:nil]) {
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }

    *absolute_byte_offset = static_cast<FLAC__uint64>(offset);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus lengthCallback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length,
                                               void *client_data) noexcept {
#pragma unused(decoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;

    NSInteger length;
    if (![flacDecoder->_inputSource getLength:&length error:nil]) {
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    }

    *stream_length = static_cast<FLAC__uint64>(length);
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool eofCallback(const FLAC__StreamDecoder *decoder, void *client_data) noexcept {
#pragma unused(decoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
    return flacDecoder->_inputSource.atEOF;
}

FLAC__StreamDecoderWriteStatus writeCallback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
                                             const FLAC__int32 *const buffer[], void *client_data) noexcept {
#pragma unused(decoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
    return [flacDecoder handleFLACWrite:decoder frame:frame buffer:buffer];
}

void metadataCallback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata,
                      void *client_data) noexcept {
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
    [flacDecoder handleFLACMetadata:decoder metadata:metadata];
}

void errorCallback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status,
                   void *client_data) noexcept {
    NSCParameterAssert(client_data != nullptr);

    SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
    [flacDecoder handleFLACError:decoder status:status];
}

} /* namespace */

@implementation SFBFLACDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"flac"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/flac"];
}

+ (SFBAudioDecoderName)decoderName {
    return SFBAudioDecoderNameFLAC;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [inputSource readHeaderOfLength:SFBFLACDetectionSize skipID3v2Tag:YES error:error];
    if (!header) {
        return NO;
    }

    if ([header isFLACHeader]) {
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else {
        *formatIsSupported = SFBTernaryTruthValueFalse;
    }

    return YES;
}

- (BOOL)decodingIsLossless {
    return YES;
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    // Create FLAC decoder
    flac__stream_decoder_unique_ptr flac{FLAC__stream_decoder_new()};
    if (!flac) {
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    // Initialize decoder
    if (![self initializeFLACStreamDecoder:flac.get() error:error]) {
        return NO;
    }

    // Process metadata
    if (!FLAC__stream_decoder_process_until_end_of_metadata(flac.get())) {
        os_log_error(gSFBAudioDecoderLog, "FLAC__stream_decoder_process_until_end_of_metadata failed: %{public}s",
                     FLAC__stream_decoder_get_resolved_state_string(flac.get()));
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeInvalidFormat,
                  NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return NO;
    }

    // FLAC supports from 4 to 32 bits per sample; this check is likely unnecessary
    if (_streamInfo.bits_per_sample < 4 || _streamInfo.bits_per_sample > 32) {
        os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %u", _streamInfo.bits_per_sample);
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                  NSLocalizedString(@"The file “%@” is not a supported FLAC file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The audio bit depth is not supported.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
    }

    _framePosition = 0;

    // Set up the processing format
    AudioStreamBasicDescription processingStreamDescription{};

    processingStreamDescription.mFormatID = kAudioFormatLinearPCM;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
    processingStreamDescription.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger |
                                               kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsAlignedHigh;
#pragma clang diagnostic pop

    processingStreamDescription.mSampleRate = _streamInfo.sample_rate;
    processingStreamDescription.mChannelsPerFrame = _streamInfo.channels;
    processingStreamDescription.mBitsPerChannel = _streamInfo.bits_per_sample;

    processingStreamDescription.mBytesPerPacket = 4;
    processingStreamDescription.mFramesPerPacket = 1;
    processingStreamDescription.mBytesPerFrame =
          processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

    _flac = std::move(flac);

    AVAudioChannelLayout *channelLayout = nil;
    switch (_streamInfo.channels) {
    case 1:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];
        break;
    case 2:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
        break;
    case 3:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_3_0_A];
        break;
    case 4:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_4_0_B];
        break;
    case 5:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_5_0_B];
        break;
    case 6:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_5_1_B];
        break;
    case 7:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_6_1];
        break;
    case 8:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_7_1];
        break;
    }

    _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription
                                                           channelLayout:channelLayout];

    // Set up the source format
    AudioStreamBasicDescription sourceStreamDescription{};

    sourceStreamDescription.mFormatID = kAudioFormatFLAC;

    sourceStreamDescription.mSampleRate = _streamInfo.sample_rate;
    sourceStreamDescription.mChannelsPerFrame = _streamInfo.channels;
    // Apple uses kAppleLosslessFormatFlag_XXBitSourceData to indicate FLAC bit depth in the Core Audio FLAC decoder
    // Since the number of flags is limited the source bit depth is also stored in mBitsPerChannel
    sourceStreamDescription.mBitsPerChannel = _streamInfo.bits_per_sample;
    switch (_streamInfo.bits_per_sample) {
    case 16:
        sourceStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
        break;
    case 20:
        sourceStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_20BitSourceData;
        break;
    case 24:
        sourceStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;
        break;
    case 32:
        sourceStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_32BitSourceData;
        break;
    }

    sourceStreamDescription.mFramesPerPacket = _streamInfo.max_blocksize;

    _sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription
                                                       channelLayout:channelLayout];

    // Populate codec properties
    _properties = @{
        SFBAudioDecodingPropertiesKeyFLACMinimumBlockSize : @(_streamInfo.min_blocksize),
        SFBAudioDecodingPropertiesKeyFLACMaximumBlockSize : @(_streamInfo.max_blocksize),
        SFBAudioDecodingPropertiesKeyFLACMinimumFrameSize : @(_streamInfo.min_framesize),
        SFBAudioDecodingPropertiesKeyFLACMaximumFrameSize : @(_streamInfo.max_framesize),
        SFBAudioDecodingPropertiesKeyFLACSampleRate : @(_streamInfo.sample_rate),
        SFBAudioDecodingPropertiesKeyFLACChannels : @(_streamInfo.channels),
        SFBAudioDecodingPropertiesKeyFLACBitsPerSample : @(_streamInfo.bits_per_sample),
        SFBAudioDecodingPropertiesKeyFLACTotalSamples : @(_streamInfo.total_samples),
        SFBAudioDecodingPropertiesKeyFLACMD5Sum : [[NSData alloc] initWithBytes:_streamInfo.md5sum length:16],
    };

    // Allocate the buffer list (which will convert from FLAC's push model to Core Audio's pull model)
    _frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat
                                                 frameCapacity:_streamInfo.max_blocksize];
    _frameBuffer.frameLength = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_flac && !FLAC__stream_decoder_finish(_flac.get())) {
        os_log_info(gSFBAudioDecoderLog, "FLAC__stream_decoder_finish failed: %{public}s",
                    FLAC__stream_decoder_get_resolved_state_string(_flac.get()));
    }

    _flac.reset();

    _frameBuffer = nil;
    memset(&_streamInfo, 0, sizeof(_streamInfo));

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _flac != nullptr;
}

- (AVAudioFramePosition)framePosition {
    return _framePosition;
}

- (AVAudioFramePosition)frameLength {
    return static_cast<AVAudioFramePosition>(_streamInfo.total_samples);
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    // Reset output buffer data size
    buffer.frameLength = 0;

    frameLength = std::min(frameLength, buffer.frameCapacity);
    if (frameLength == 0) {
        return YES;
    }

    AVAudioFrameCount framesProcessed = 0;

    for (;;) {
        AVAudioFrameCount framesRemaining = frameLength - framesProcessed;
        AVAudioFrameCount framesCopied = [buffer appendFromBuffer:_frameBuffer
                                                readingFromOffset:0
                                                      frameLength:framesRemaining];
        [_frameBuffer trimAtOffset:0 frameLength:framesCopied];

        framesProcessed += framesCopied;

        // All requested frames were read or EOS reached
        if (framesProcessed == frameLength ||
            FLAC__stream_decoder_get_state(_flac.get()) == FLAC__STREAM_DECODER_END_OF_STREAM) {
            break;
        }

        // Grab the next frame
        if (!FLAC__stream_decoder_process_single(_flac.get())) {
            os_log_error(gSFBAudioDecoderLog, "FLAC__stream_decoder_process_single failed: %{public}s",
                         FLAC__stream_decoder_get_resolved_state_string(_flac.get()));
            if (error) {
                if (_writeError) {
                    *error = _writeError;
                } else {
                    *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                 code:SFBAudioDecoderErrorCodeDecodingError
                                             userInfo:@{NSURLErrorKey : _inputSource.url}];
                }
            }
            return NO;
        }
    }

    _framePosition += framesProcessed;

    return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    NSParameterAssert(frame >= 0);
    //    NSParameterAssert(frame <= _totalFrames);

    auto result = FLAC__stream_decoder_seek_absolute(_flac.get(), static_cast<FLAC__uint64>(frame));

    // Attempt to re-sync the stream if necessary
    if (!result && FLAC__stream_decoder_get_state(_flac.get()) == FLAC__STREAM_DECODER_SEEK_ERROR) {
        os_log_debug(gSFBAudioDecoderLog, "FLAC seek error, attempting re-sync");
        result = FLAC__stream_decoder_flush(_flac.get());
    }

    if (!result) {
        os_log_error(gSFBAudioDecoderLog, "FLAC seek error: %{public}s",
                     FLAC__stream_decoder_get_resolved_state_string(_flac.get()));
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                         code:SFBAudioDecoderErrorCodeSeekError
                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
        }
        return NO;
    }

    _framePosition = frame;
    return YES;
}

- (BOOL)initializeFLACStreamDecoder:(FLAC__StreamDecoder *)decoder error:(NSError **)error {
    auto status = FLAC__stream_decoder_init_stream(decoder, readCallback, seekCallback, tellCallback, lengthCallback,
                                                   eofCallback, writeCallback, metadataCallback, errorCallback,
                                                   (__bridge void *)self);
    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        os_log_error(gSFBAudioDecoderLog, "FLAC__stream_decoder_init_stream failed: %{public}s",
                     FLAC__stream_decoder_get_resolved_state_string(decoder));
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeInvalidFormat,
                  NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return NO;
    }

    return YES;
}

- (FLAC__StreamDecoderWriteStatus)handleFLACWrite:(const FLAC__StreamDecoder *)decoder
                                            frame:(const FLAC__Frame *)frame
                                           buffer:(const FLAC__int32 *const[])buffer {
#if DEBUG
    NSParameterAssert(decoder != nullptr);
    NSParameterAssert(frame != nullptr);
#endif /* DEBUG */

    // Changes in channel count or sample rate mid-stream are not supported
    if (const auto firstFrame = frame->header.number.sample_number == 0; !firstFrame) {
        if (frame->header.channels != _previousFrameHeader.channels) {
            os_log_error(gSFBAudioDecoderLog, "Change in channel count from %d to %d detected",
                         _previousFrameHeader.channels, frame->header.channels);

            _writeError = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                  NSLocalizedString(@"The file “%@” is not a supported FLAC file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"Changes in channel count are not supported.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));

            _frameBuffer.frameLength = 0;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        if (frame->header.sample_rate != _previousFrameHeader.sample_rate) {
            os_log_error(gSFBAudioDecoderLog, "Change in sample rate from %g kHz to %g kHz detected",
                         static_cast<double>(_previousFrameHeader.sample_rate) / 1000.0,
                         static_cast<double>(frame->header.sample_rate) / 1000.0);

            _writeError = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                  NSLocalizedString(@"The file “%@” is not a supported FLAC file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"Changes in sample rate are not supported.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));

            _frameBuffer.frameLength = 0;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        if (frame->header.bits_per_sample != _previousFrameHeader.bits_per_sample) {
            os_log_debug(gSFBAudioDecoderLog, "Change in audio bit depth from %d to %d detected",
                         _previousFrameHeader.bits_per_sample, frame->header.bits_per_sample);
        }
    }

    const auto *abl = _frameBuffer.audioBufferList;
    assert(abl->mNumberBuffers == frame->header.channels);

    // FLAC hands us 32-bit signed integers with the samples low-aligned
    if (frame->header.bits_per_sample != 32) [[likely]] {
        // Shift the samples to high alignment
        const auto shift = 32 - frame->header.bits_per_sample;
        const auto channels = frame->header.channels;
        const auto blocksize = frame->header.blocksize;

        for (uint32_t channel = 0; channel < channels; ++channel) {
            // simd_uint8 and simd_uint16 require 16 byte alignment
            using simd_vector = simd_uint16;
            using simd_packed_vector = simd_packed_uint16;
            constexpr uint32_t simd_vector_size = 16;

            uint32_t *__restrict dst = static_cast<uint32_t *>(abl->mBuffers[channel].mData);
            const FLAC__int32 *__restrict src = buffer[channel];

            uint32_t sample = 0;
            if (blocksize > simd_vector_size) {
                for (; sample <= blocksize - simd_vector_size; sample += simd_vector_size) {
                    simd_vector v = *reinterpret_cast<const simd_packed_vector *>(&src[sample]);
                    v <<= shift;
                    *reinterpret_cast<simd_packed_vector *>(&dst[sample]) = v;
                }
            }

            for (; sample < blocksize; ++sample) {
                dst[sample] = static_cast<uint32_t>(src[sample]) << shift;
            }
        }
    } else {
        for (uint32_t channel = 0; channel < frame->header.channels; ++channel) {
            memcpy(abl->mBuffers[channel].mData, buffer[channel], frame->header.blocksize * sizeof(FLAC__int32));
        }
    }

    _frameBuffer.frameLength = frame->header.blocksize;
    _previousFrameHeader = frame->header;

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

- (void)handleFLACMetadata:(const FLAC__StreamDecoder *)decoder metadata:(const FLAC__StreamMetadata *)metadata {
    NSParameterAssert(metadata != nullptr);

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        memcpy(&_streamInfo, &metadata->data.stream_info, sizeof(metadata->data.stream_info));
    }
}

- (void)handleFLACError:(const FLAC__StreamDecoder *)decoder status:(FLAC__StreamDecoderErrorStatus)status {
    os_log_error(gSFBAudioDecoderLog, "FLAC error: %{public}s", FLAC__StreamDecoderErrorStatusString[status]);
}

@end

@implementation SFBOggFLACDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"oga"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=flac"];
}

+ (SFBAudioDecoderName)decoderName {
    return SFBAudioDecoderNameOggFLAC;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [inputSource readHeaderOfLength:SFBOggFLACDetectionSize skipID3v2Tag:NO error:error];
    if (!header) {
        return NO;
    }

    if ([header isOggFLACHeader]) {
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else {
        *formatIsSupported = SFBTernaryTruthValueFalse;
    }

    return YES;
}

- (BOOL)initializeFLACStreamDecoder:(FLAC__StreamDecoder *)decoder error:(NSError **)error {

    auto status = FLAC__stream_decoder_init_ogg_stream(decoder, readCallback, seekCallback, tellCallback,
                                                       lengthCallback, eofCallback, writeCallback, metadataCallback,
                                                       errorCallback, (__bridge void *)self);

    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        os_log_error(gSFBAudioDecoderLog, "FLAC__stream_decoder_init_ogg_stream failed: %{public}s",
                     FLAC__stream_decoder_get_resolved_state_string(decoder));
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeInvalidFormat,
                  NSLocalizedString(@"The file “%@” is not a valid Ogg FLAC file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return NO;
    }

    return YES;
}

@end
