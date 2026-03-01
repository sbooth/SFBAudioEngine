//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBMPEGDecoder.h"

#import "SFBLocalizedNameForURL.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>
#import <mpg123/mpg123.h>

#import <os/log.h>

SFBAudioDecoderName const SFBAudioDecoderNameMPEG = @"org.sbooth.AudioEngine.Decoder.MPEG";

// ========================================
// Callbacks
static int read_callback(void *iohandle, void *ptr, size_t size, size_t *read) {
    NSCParameterAssert(iohandle != NULL);

    SFBMPEGDecoder *decoder = (__bridge SFBMPEGDecoder *)iohandle;

    NSInteger bytesRead;
    if (![decoder->_inputSource readBytes:ptr length:(NSInteger)size bytesRead:&bytesRead error:nil]) {
        return -1;
    }
    *read = bytesRead;
    return 0;
}

static int64_t lseek_callback(void *iohandle, int64_t offset, int whence) {
    NSCParameterAssert(iohandle != NULL);

    SFBMPEGDecoder *decoder = (__bridge SFBMPEGDecoder *)iohandle;

    if (!decoder->_inputSource.supportsSeeking) {
        return -1;
    }

    // Adjust offset as required
    switch (whence) {
    case SEEK_SET:
        // offset remains unchanged
        break;
    case SEEK_CUR: {
        NSInteger inputSourceOffset;
        if ([decoder->_inputSource getOffset:&inputSourceOffset error:nil]) {
            offset += inputSourceOffset;
        }
        break;
    }
    case SEEK_END: {
        NSInteger inputSourceLength;
        if ([decoder->_inputSource getLength:&inputSourceLength error:nil]) {
            offset += inputSourceLength;
        }
        break;
    }
    }

    if (![decoder->_inputSource seekToOffset:offset error:nil]) {
        return -1;
    }

    return offset;
}

/// Returns true if @c buf appears to be an ID3v2 tag header.
/// @warning @c buf must be at least 10 bytes in size.
static BOOL is_id3v2_tag_header(const unsigned char *buf) {
    /*
     An ID3v2 tag can be detected with the following pattern:
     $49 44 33 yy yy xx zz zz zz zz
     Where yy is less than $FF, xx is the 'flags' byte and zz is less than
     $80.
     */

    if (buf[0] != 0x49 || buf[1] != 0x44 || buf[2] != 0x33) {
        return NO;
    }
    if (buf[3] >= 0xff || buf[4] >= 0xff) {
        return NO;
    }
    if (buf[5] & 0xf) {
        return NO;
    }
    if (buf[6] >= 0x80 || buf[7] >= 0x80 || buf[8] >= 0x80 || buf[9] >= 0x80) {
        return NO;
    }
    return YES;
}

/// Returns the total size in bytes of the ID3v2 tag with @c header.
/// @warning @c header must be at least 10 bytes in size.
static uint32_t id3v2_tag_total_size(const unsigned char *header) {
    unsigned char flags = header[5];
    // The size is stored as a 32-bit synchsafe integer with 28 effective bits
    uint32_t size = (header[6] << 21) | (header[7] << 14) | (header[8] << 7) | header[9];
    return 10 + size + (flags & 0x10 ? 10 : 0);
}

/// Searches for an MP3 sync word and minimal valid frame header in @c buf.
static BOOL contains_mp3_sync_word_and_minimal_valid_frame_header(const unsigned char *buf, NSInteger len) {
    NSCParameterAssert(buf != NULL);
    NSCParameterAssert(len >= 3);

    const unsigned char *loc = buf;
    for (;;) {
        // Search for first byte of MP3 sync word
        loc = (const unsigned char *)memchr(loc, 0xff, len - (loc - buf) - 2);
        if (!loc) {
            break;
        }

        // Check whether a complete MP3 sync word was found and perform a minimal check for a valid MP3 frame header
        if ((*(loc + 1) & 0xe0) == 0xe0 && (*(loc + 1) & 0x18) != 0x08 && (*(loc + 1) & 0x06) != 0 &&
            (*(loc + 2) & 0xf0) != 0xf0 && (*(loc + 2) & 0x0c) != 0x0c) {
            return YES;
        }

        loc++;
    }

    return NO;
}

@interface SFBMPEGDecoder () {
  @private
    mpg123_handle *_mpg123;
    AVAudioFramePosition _framePosition;
    AVAudioPCMBuffer *_buffer;
}
@end

@implementation SFBMPEGDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"mp3"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/mpeg"];
}

+ (SFBAudioDecoderName)decoderName {
    return SFBAudioDecoderNameMPEG;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != NULL);

    NSInteger originalOffset;
    if (![inputSource getOffset:&originalOffset error:error]) {
        return NO;
    }

    if (![inputSource seekToOffset:0 error:error]) {
        return NO;
    }

    unsigned char buf[512];
    NSInteger len;
    if (![inputSource readBytes:buf length:sizeof buf bytesRead:&len error:error]) {
        return NO;
    }

    NSInteger searchStartOffset = 0;

    // Attempt to detect and minimally parse an ID3v2 tag header
    if (len >= 10 && is_id3v2_tag_header(buf)) {
        searchStartOffset = id3v2_tag_total_size(buf);

        // Skip tag data

        // Ensure 3 bytes are available for MP3 frame header check
        if (searchStartOffset <= len - 3) {
            memmove(buf, buf + searchStartOffset, len - searchStartOffset);
            len -= searchStartOffset;
        } else {
            if (![inputSource seekToOffset:searchStartOffset error:error]) {
                return NO;
            }

            // Read next chunk
            if (![inputSource readBytes:buf length:sizeof buf bytesRead:&len error:error]) {
                return NO;
            }
        }
    }

    // Search for an MP3 sync word and a frame header that appears to be valid
    for (;;) {
        if (len < 3) {
            *formatIsSupported = SFBTernaryTruthValueFalse;
            break;
        }

        if (contains_mp3_sync_word_and_minimal_valid_frame_header(buf, len)) {
            *formatIsSupported = SFBTernaryTruthValueTrue;
            break;
        }

        // The penultimate or final byte in buf could be an undetected frame start,
        // so copy them to the beginning to ensure a continuous search
        memmove(buf, buf + len - 2, 2);
        if (![inputSource readBytes:buf + 2 length:sizeof buf - 2 bytesRead:&len error:error]) {
            return NO;
        }
        len += 2;

        // Limit searches to 2 KB
        NSInteger currentOffset;
        if (![inputSource getOffset:&currentOffset error:error]) {
            return NO;
        }

        if (currentOffset > searchStartOffset + 2048) {
            *formatIsSupported = SFBTernaryTruthValueUnknown;
            break;
        }
    }

    if (![inputSource seekToOffset:originalOffset error:error]) {
        return NO;
    }

    return YES;
}

- (BOOL)decodingIsLossless {
    return NO;
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    _mpg123 = mpg123_new(NULL, NULL);

    if (!_mpg123) {
        if (error) {
            *error = [self invalidFormatError:NSLocalizedString(@"MP3", @"")];
        }
        return NO;
    }

    // Force decode to floating point instead of 16-bit signed integer
    mpg123_param2(_mpg123, MPG123_FLAGS, MPG123_FORCE_FLOAT | MPG123_SKIP_ID3V2 | MPG123_GAPLESS | MPG123_QUIET, 0);
    mpg123_param2(_mpg123, MPG123_RESYNC_LIMIT, 2048, 0);

    if (mpg123_reader64(_mpg123, read_callback, _inputSource.supportsSeeking ? lseek_callback : NULL, NULL) != MPG123_OK) {
        mpg123_delete(_mpg123);
        _mpg123 = NULL;

        if (error) {
            *error = [self invalidFormatError:NSLocalizedString(@"MP3", @"")];
        }
        return NO;
    }

    if (mpg123_open_handle(_mpg123, (__bridge void *)self) != MPG123_OK) {
        mpg123_delete(_mpg123);
        _mpg123 = NULL;

        if (error) {
            *error = [self invalidFormatError:NSLocalizedString(@"MP3", @"")];
        }
        return NO;
    }

    _framePosition = 0;

    long rate;
    int channels;
    int encoding;
    if (mpg123_getformat(_mpg123, &rate, &channels, &encoding) != MPG123_OK || encoding != MPG123_ENC_FLOAT_32 ||
        channels <= 0) {
        mpg123_close(_mpg123);
        mpg123_delete(_mpg123);
        _mpg123 = NULL;

        if (error) {
            *error = [self invalidFormatError:NSLocalizedString(@"MP3", @"")];
        }
        return NO;
    }

    AVAudioChannelLayout *channelLayout = nil;
    switch (channels) {
    case 1:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];
        break;
    case 2:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
        break;
    default:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | (UInt32)channels)];
        break;
    }

    _processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                         sampleRate:rate
                                                        interleaved:NO
                                                      channelLayout:channelLayout];

    size_t bufferSizeBytes = mpg123_outblock(_mpg123);
    UInt32 framesPerMPEGFrame = (UInt32)(bufferSizeBytes / ((size_t)channels * sizeof(float)));

    // Set up the source format
    AudioStreamBasicDescription sourceStreamDescription = {0};

    sourceStreamDescription.mFormatID = kAudioFormatMPEGLayer3;

    struct mpg123_frameinfo2 mi;
    if (mpg123_info2(_mpg123, &mi) == MPG123_OK) {
        switch (mi.layer) {
        case 1:
            sourceStreamDescription.mFormatID = kAudioFormatMPEGLayer1;
            break;
        case 2:
            sourceStreamDescription.mFormatID = kAudioFormatMPEGLayer2;
            break;
        case 3:
            sourceStreamDescription.mFormatID = kAudioFormatMPEGLayer3;
            break;
        }
    }

    sourceStreamDescription.mSampleRate = rate;
    sourceStreamDescription.mChannelsPerFrame = (UInt32)channels;

    sourceStreamDescription.mFramesPerPacket = framesPerMPEGFrame;

    _sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription
                                                       channelLayout:channelLayout];

    if (_inputSource.supportsSeeking && mpg123_scan(_mpg123) != MPG123_OK) {
        mpg123_close(_mpg123);
        mpg123_delete(_mpg123);
        _mpg123 = NULL;

        if (error) {
            *error = [self invalidFormatError:NSLocalizedString(@"MP3", @"")];
        }
        return NO;
    }

    _buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:framesPerMPEGFrame];
    _buffer.frameLength = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_mpg123) {
        mpg123_close(_mpg123);
        mpg123_delete(_mpg123);
        _mpg123 = NULL;
    }
    _buffer = nil;

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _mpg123 != NULL;
}

- (AVAudioFramePosition)framePosition {
    return _framePosition;
}

- (AVAudioFramePosition)frameLength {
    int64_t length = mpg123_length64(_mpg123);
    if (length == MPG123_ERR) {
        return SFBUnknownFrameLength;
    }
    return length;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    // Reset output buffer data size
    buffer.frameLength = 0;

    frameLength = MIN(frameLength, buffer.frameCapacity);
    if (frameLength == 0) {
        return YES;
    }

    AVAudioFrameCount framesProcessed = 0;

    for (;;) {
        AVAudioFrameCount framesRemaining = frameLength - framesProcessed;
        AVAudioFrameCount framesCopied = [buffer appendFromBuffer:_buffer
                                                readingFromOffset:0
                                                      frameLength:framesRemaining];
        [_buffer trimAtOffset:0 frameLength:framesCopied];

        framesProcessed += framesCopied;

        // All requested frames were read
        if (framesProcessed == frameLength) {
            break;
        }

        // Read and decode an MPEG frame
        off_t frameNumber;
        unsigned char *audioData = NULL;
        size_t bytesDecoded = 0;
        int result = mpg123_decode_frame(_mpg123, &frameNumber, &audioData, &bytesDecoded);
        // EOS
        if (result == MPG123_DONE) {
            break;
        }
        if (result != MPG123_OK) {
            os_log_error(gSFBAudioDecoderLog, "mpg123_decode_frame failed: %{public}s", mpg123_strerror(_mpg123));
            if (error) {
                *error = [self genericDecodingError];
            }
            return NO;
        }

        // Deinterleave the samples
        AVAudioFrameCount framesDecoded =
                (AVAudioFrameCount)(bytesDecoded / (sizeof(float) * _buffer.format.channelCount));

        float *const *floatChannelData = _buffer.floatChannelData;
        AVAudioChannelCount channelCount = _buffer.format.channelCount;
        for (AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
            const float *input = (float *)audioData + channel;
            float *output = floatChannelData[channel];
            for (AVAudioFrameCount frame = 0; frame < framesDecoded; ++frame) {
                *output++ = *input;
                input += channelCount;
            }
        }

        _buffer.frameLength = framesDecoded;
    }

    _framePosition += framesProcessed;

    return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    NSParameterAssert(frame >= 0);

    off_t offset = mpg123_seek(_mpg123, frame, SEEK_SET);
    if (offset < 0) {
        os_log_error(gSFBAudioDecoderLog, "mpg123 seek error");
        if (error) {
            *error = [self genericSeekError];
        }
        return NO;
    }

    _framePosition = offset;
    return offset >= 0;
}

@end
