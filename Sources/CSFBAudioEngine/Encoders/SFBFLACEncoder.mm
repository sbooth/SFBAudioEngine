//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBFLACEncoder.h"

#import <FLAC/metadata.h>
#import <FLAC/stream_encoder.h>

#import <os/log.h>

#import <algorithm>
#import <memory>

SFBAudioEncoderName const SFBAudioEncoderNameFLAC = @"org.sbooth.AudioEngine.Encoder.FLAC";
SFBAudioEncoderName const SFBAudioEncoderNameOggFLAC = @"org.sbooth.AudioEngine.Encoder.OggFLAC";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACCompressionLevel = @"Compression Level";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACVerifyEncoding = @"Verify Encoding";

namespace {

constexpr uint32_t kDefaultPaddingSize = 8192;

/// A `std::unique_ptr` deleter for `FLAC__StreamEncoder` objects
struct flac__stream_encoder_deleter {
    void operator()(FLAC__StreamEncoder *encoder) { FLAC__stream_encoder_delete(encoder); }
};

/// A `std::unique_ptr` deleter for `FLAC__StreamMetadata` objects
struct flac__stream_metadata_deleter {
    void operator()(FLAC__StreamMetadata *metadata) { FLAC__metadata_object_delete(metadata); }
};

using flac__stream_encoder_unique_ptr = std::unique_ptr<FLAC__StreamEncoder, flac__stream_encoder_deleter>;
using flac__stream_metadata_unique_ptr = std::unique_ptr<FLAC__StreamMetadata, flac__stream_metadata_deleter>;

} /* namespace */

@interface SFBFLACEncoder () {
  @private
    flac__stream_encoder_unique_ptr _flac;
    flac__stream_metadata_unique_ptr _seektable;
    flac__stream_metadata_unique_ptr _padding;
    FLAC__StreamMetadata *_metadata[2];
  @package
    AVAudioFramePosition _framePosition;
}
- (BOOL)initializeFLACStreamEncoder:(FLAC__StreamEncoder *)encoder error:(NSError **)error;
@end

// MARK: FLAC Callbacks

namespace {

FLAC__StreamEncoderReadStatus readCallback(const FLAC__StreamEncoder *encoder, FLAC__byte buffer[], size_t *bytes,
                                           void *client_data) noexcept {
#pragma unused(encoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
    SFBOutputTarget *outputTarget = flacEncoder->_outputTarget;

    NSInteger bytesRead;
    if (![outputTarget readBytes:buffer length:static_cast<NSInteger>(*bytes) bytesRead:&bytesRead error:nil]) {
        return FLAC__STREAM_ENCODER_READ_STATUS_ABORT;
    }

    *bytes = static_cast<size_t>(bytesRead);

    if (bytesRead == 0 && outputTarget.atEOF) {
        return FLAC__STREAM_ENCODER_READ_STATUS_END_OF_STREAM;
    }

    return FLAC__STREAM_ENCODER_READ_STATUS_CONTINUE;
}

FLAC__StreamEncoderWriteStatus writeCallback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[],
                                             size_t bytes, uint32_t samples, uint32_t current_frame,
                                             void *client_data) noexcept {
#pragma unused(encoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
    SFBOutputTarget *outputTarget = flacEncoder->_outputTarget;

    NSInteger bytesWritten;
    if (![outputTarget writeBytes:static_cast<const void *>(buffer)
                           length:static_cast<NSInteger>(bytes)
                     bytesWritten:&bytesWritten
                            error:nil] ||
        bytesWritten != static_cast<NSInteger>(bytes)) {
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }

    if (samples > 0) {
        flacEncoder->_framePosition = current_frame;
    }

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__StreamEncoderSeekStatus seekCallback(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset,
                                           void *client_data) noexcept {
#pragma unused(encoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
    SFBOutputTarget *outputTarget = flacEncoder->_outputTarget;

    if (!outputTarget.supportsSeeking) {
        return FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;
    }

    if (![outputTarget seekToOffset:static_cast<NSInteger>(absolute_byte_offset) error:nil]) {
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    }

    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

FLAC__StreamEncoderTellStatus tellCallback(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset,
                                           void *client_data) noexcept {
#pragma unused(encoder)
    NSCParameterAssert(client_data != nullptr);

    SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
    SFBOutputTarget *outputTarget = flacEncoder->_outputTarget;

    NSInteger offset;
    if (![outputTarget getOffset:&offset error:nil]) {
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    }

    *absolute_byte_offset = static_cast<FLAC__uint64>(offset);

    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

void metadataCallback(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata,
                      void *client_data) noexcept {
#pragma unused(encoder)
#pragma unused(metadata)
#pragma unused(client_data)
}

} /* namespace */

@implementation SFBFLACEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"flac"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/flac"];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameFLAC;
}

- (BOOL)encodingIsLossless {
    return YES;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat {
    NSParameterAssert(sourceFormat != nil);

    // Validate format
    if ((sourceFormat.streamDescription->mFormatFlags & kAudioFormatFlagIsFloat) == kAudioFormatFlagIsFloat ||
        sourceFormat.channelCount < 1 || sourceFormat.channelCount > 8) {
        return nil;
    }

    // Set up the processing format
    AudioStreamBasicDescription streamDescription{};

    streamDescription.mFormatID = kAudioFormatLinearPCM;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
    streamDescription.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;
#pragma clang diagnostic pop

    streamDescription.mSampleRate = sourceFormat.sampleRate;
    streamDescription.mChannelsPerFrame = sourceFormat.channelCount;
    streamDescription.mBitsPerChannel = ((sourceFormat.streamDescription->mBitsPerChannel + 7) / 8) * 8;
    if (streamDescription.mBitsPerChannel == 32) {
        streamDescription.mFormatFlags |= kAudioFormatFlagIsPacked;
    } else {
        streamDescription.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
    }

    streamDescription.mBytesPerPacket = sizeof(int32_t) * streamDescription.mChannelsPerFrame;
    streamDescription.mFramesPerPacket = 1;
    streamDescription.mBytesPerFrame = streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

    AVAudioChannelLayout *channelLayout = nil;
    switch (sourceFormat.channelCount) {
    case 1:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];
        break;
    case 2:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
        break;
    case 3:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_3_0];
        break;
    case 4:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_4_0_B];
        break;
    case 5:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_5_0_A];
        break;
    case 6:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_5_1_A];
        break;
    case 7:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_6_1];
        break;
    case 8:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_WAVE_7_1];
        break;
    }

    return [[AVAudioFormat alloc] initWithStreamDescription:&streamDescription channelLayout:channelLayout];
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    // Create FLAC encoder
    flac__stream_encoder_unique_ptr flac{FLAC__stream_encoder_new()};
    if (!flac) {
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    // Output format
    // As long as the FLAC encoder is non-null and uninitialized these setters will succeed
    FLAC__stream_encoder_set_sample_rate(flac.get(), static_cast<uint32_t>(_processingFormat.sampleRate));
    FLAC__stream_encoder_set_channels(flac.get(), _processingFormat.channelCount);
    FLAC__stream_encoder_set_bits_per_sample(flac.get(), _processingFormat.streamDescription->mBitsPerChannel);
    if (_estimatedFramesToEncode > 0) {
        FLAC__stream_encoder_set_total_samples_estimate(flac.get(),
                                                        static_cast<FLAC__uint64>(_estimatedFramesToEncode));
    }

    // Encoder compression level
    if (NSNumber *compressionLevel = [_settings objectForKey:SFBAudioEncodingSettingsKeyFLACCompressionLevel];
        compressionLevel != nil) {
        unsigned int value = compressionLevel.unsignedIntValue;
        if (value >= 0 && value <= 8) {
            if (!FLAC__stream_encoder_set_compression_level(flac.get(), value)) {
                os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_compression_level failed: %{public}s",
                             FLAC__stream_encoder_get_resolved_state_string(flac.get()));
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }
        } else {
            os_log_info(gSFBAudioEncoderLog, "Ignoring invalid FLAC compression level: %d", value);
        }
    }

    if (NSNumber *verifyEncoding = [_settings objectForKey:SFBAudioEncodingSettingsKeyFLACVerifyEncoding];
        verifyEncoding != nil) {
        FLAC__stream_encoder_set_verify(flac.get(), verifyEncoding.boolValue != 0);
    }

    // Create the padding metadata block
    flac__stream_metadata_unique_ptr padding{FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)};
    if (!padding) {
        os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_new failed");
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    padding->length = kDefaultPaddingSize;

    // Create a seektable when possible
    flac__stream_metadata_unique_ptr seektable;
    if (_estimatedFramesToEncode > 0) {
        seektable = flac__stream_metadata_unique_ptr{FLAC__metadata_object_new(FLAC__METADATA_TYPE_SEEKTABLE)};
        if (!seektable) {
            os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_new failed");
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }

        // Append seekpoints (one every 10 seconds)
        if (!FLAC__metadata_object_seektable_template_append_spaced_points_by_samples(
                    seektable.get(), static_cast<uint32_t>(10 * _processingFormat.sampleRate),
                    static_cast<FLAC__uint64>(_estimatedFramesToEncode))) {
            os_log_error(gSFBAudioEncoderLog,
                         "FLAC__metadata_object_seektable_template_append_spaced_points_by_samples failed");
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }

        // Sort the table
        if (!FLAC__metadata_object_seektable_template_sort(seektable.get(), false)) {
            os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_seektable_template_sort failed");
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }
    }

    _metadata[0] = padding.get();
    if (seektable) {
        _metadata[1] = seektable.get();
    }

    if (!FLAC__stream_encoder_set_metadata(flac.get(), _metadata, seektable ? 2 : 1)) {
        os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_metadata failed: %{public}s",
                     FLAC__stream_encoder_get_resolved_state_string(flac.get()));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    // Initialize the FLAC encoder
    if (![self initializeFLACStreamEncoder:flac.get() error:error]) {
        return NO;
    }

    AudioStreamBasicDescription outputStreamDescription{};
    outputStreamDescription.mFormatID = kAudioFormatFLAC;
    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;
    outputStreamDescription.mBitsPerChannel = _processingFormat.streamDescription->mBitsPerChannel;
    switch (outputStreamDescription.mBitsPerChannel) {
    case 16:
        outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
        break;
    case 20:
        outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_20BitSourceData;
        break;
    case 24:
        outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;
        break;
    case 32:
        outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_32BitSourceData;
        break;
    }
    outputStreamDescription.mFramesPerPacket = FLAC__stream_encoder_get_blocksize(flac.get());
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription
                                                       channelLayout:_processingFormat.channelLayout];

    _flac = std::move(flac);
    _seektable = std::move(seektable);
    _padding = std::move(padding);

    _framePosition = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _flac.reset();
    _seektable.reset();
    _padding.reset();

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _flac != nullptr;
}

- (AVAudioFramePosition)framePosition {
    return _framePosition;
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    frameLength = std::min(frameLength, buffer.frameLength);
    if (frameLength == 0) {
        return YES;
    }

    // The libFLAC encoder expects signed 32-bit samples in the range of the audio bit depth
    // (e.g. for 16 bit samples the interval is [-32768, 32767]).
    //
    // Samples in the processing format needs to be massaged slightly before the handoff
    // to libFLAC.

    // Probably unnecessary sanity check
    static_assert(std::is_same_v<int32_t, FLAC__int32>, "int32_t and FLAC__int32 are different types");

    // Ensure implementation-defined right shift for negative numbers is arithmetic
    static_assert(~0 >> 1 == ~0, "signed right shift is not arithmetic");

    const auto *const format = _processingFormat.streamDescription;
    if (const auto bits = format->mBitsPerChannel; bits != 32) {
        int32_t dst[512];
        const AVAudioFrameCount frameCapacity = sizeof(dst) / format->mBytesPerFrame;

        const auto shift = 32 - bits;
        const auto stride = buffer.stride;

        auto framesRemaining = frameLength;
        while (framesRemaining > 0) {
            const auto frameCount = std::min(frameCapacity, framesRemaining);

            const auto frameOffset = frameLength - framesRemaining;
            const auto byteOffset = frameOffset * format->mBytesPerFrame;
            auto *const src = reinterpret_cast<int32_t *>(
                    static_cast<unsigned char *>(buffer.audioBufferList->mBuffers[0].mData) + byteOffset);

            // Shift from high alignment, sign extending in the process
            for (AVAudioFrameCount i = 0; i < frameCount * stride; ++i) {
                dst[i] = src[i] >> shift;
            }

            if (!FLAC__stream_encoder_process_interleaved(_flac.get(), dst, frameCount)) {
                os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_process_interleaved failed: %{public}s",
                             FLAC__stream_encoder_get_resolved_state_string(_flac.get()));
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }

            framesRemaining -= frameCount;
        }
    } else {
        // Pass 32-bit samples straight through
        if (!FLAC__stream_encoder_process_interleaved(
                    _flac.get(), static_cast<int32_t *>(buffer.audioBufferList->mBuffers[0].mData), frameLength)) {
            os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_process_interleaved failed: %{public}s",
                         FLAC__stream_encoder_get_resolved_state_string(_flac.get()));
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    if (!FLAC__stream_encoder_finish(_flac.get())) {
        os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s",
                     FLAC__stream_encoder_get_resolved_state_string(_flac.get()));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
    return YES;
}

- (BOOL)initializeFLACStreamEncoder:(FLAC__StreamEncoder *)encoder error:(NSError **)error {
    NSParameterAssert(encoder != nullptr);

    auto encoderStatus = FLAC__stream_encoder_init_stream(encoder, writeCallback, seekCallback, tellCallback,
                                                          metadataCallback, (__bridge void *)self);
    if (encoderStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_init_stream failed: %{public}s",
                     FLAC__stream_encoder_get_resolved_state_string(encoder));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    return YES;
}

@end

@implementation SFBOggFLACEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"oga"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=flac"];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameOggFLAC;
}

- (BOOL)encodingIsLossless {
    return YES;
}

- (BOOL)initializeFLACStreamEncoder:(FLAC__StreamEncoder *)encoder error:(NSError **)error {
    NSParameterAssert(encoder != nullptr);

    // As long as the FLAC encoder is non-null and uninitialized this setter will succeed
    FLAC__stream_encoder_set_ogg_serial_number(encoder, static_cast<int>(arc4random()));

    auto encoderStatus = FLAC__stream_encoder_init_ogg_stream(encoder, readCallback, writeCallback, seekCallback,
                                                              tellCallback, metadataCallback, (__bridge void *)self);
    if (encoderStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_init_ogg_stream failed: %{public}s",
                     FLAC__stream_encoder_get_resolved_state_string(encoder));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    return YES;
}

@end
