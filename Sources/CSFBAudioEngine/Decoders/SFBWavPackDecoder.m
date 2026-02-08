//
// SPDX-FileCopyrightText: 2011 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBWavPackDecoder.h"

#import "NSData+SFBExtensions.h"
#import "SFBLocalizedNameForURL.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>
#import <wavpack/wavpack.h>

#import <AudioToolbox/AudioToolbox.h>

#import <os/log.h>

SFBAudioDecoderName const SFBAudioDecoderNameWavPack = @"org.sbooth.AudioEngine.Decoder.WavPack";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackMode = @"WavpackGetMode";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackQualifyMode = @"WavpackGetQualifyMode";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackVersion = @"WavpackGetVersion";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackFileFormat = @"WavpackGetFileFormat";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNumberSamples = @"WavpackGetNumSamples64";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNumberSamplesInFrame =
        @"WavpackGetNumSamplesInFrame";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackSampleRate = @"WavpackGetSampleRate";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNativeSampleRate =
        @"WavpackGetNativeSampleRate";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackBitsPerSample = @"WavpackGetBitsPerSample";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackBytesPerSample = @"WavpackGetBytesPerSample";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNumberChannels = @"WavpackGetNumChannels";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackChannelMask = @"WavpackGetChannelMask";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackReducedChannels = @"WavpackGetReducedChannels";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackFloatNormExponent = @"WavpackGetFloatNormExp";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackRatio = @"WavpackGetRatio";

#define BUFFER_SIZE_FRAMES 2048

static int32_t read_bytes_callback(void *id, void *data, int32_t bcount) {
    NSCParameterAssert(id != NULL);

    SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

    NSInteger bytesRead;
    if (![decoder->_inputSource readBytes:data length:bcount bytesRead:&bytesRead error:nil]) {
        return -1;
    }
    return (int32_t)bytesRead;
}

static int64_t get_pos_callback(void *id) {
    NSCParameterAssert(id != NULL);

    SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

    NSInteger offset;
    if (![decoder->_inputSource getOffset:&offset error:nil]) {
        return 0;
    }
    return offset;
}

static int set_pos_abs_callback(void *id, int64_t pos) {
    NSCParameterAssert(id != NULL);

    SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;
    return ![decoder->_inputSource seekToOffset:pos error:nil];
}

static int set_pos_rel_callback(void *id, int64_t delta, int mode) {
    NSCParameterAssert(id != NULL);

    SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

    if (!decoder->_inputSource.supportsSeeking) {
        return -1;
    }

    // Adjust offset as required
    NSInteger offset = delta;
    switch (mode) {
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

    return ![decoder->_inputSource seekToOffset:offset error:nil];
}

// FIXME: How does one emulate ungetc when the data is non-seekable?
// A small read buffer in SFBWavPackDecoder would work but this function
// only seems to be called once per file (when opening) so it may not be worthwhile
static int push_back_byte_callback(void *id, int c) {
    NSCParameterAssert(id != NULL);

    SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

    if (!decoder->_inputSource.supportsSeeking) {
        return EOF;
    }

    NSInteger offset;
    if (![decoder->_inputSource getOffset:&offset error:nil] || offset < 1) {
        return EOF;
    }

    if (![decoder->_inputSource seekToOffset:(offset - 1) error:nil]) {
        return EOF;
    }

    return c;
}

static int64_t get_length_callback(void *id) {
    NSCParameterAssert(id != NULL);

    SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

    NSInteger length;
    if (![decoder->_inputSource getLength:&length error:nil]) {
        return -1;
    }
    return length;
}

static int can_seek_callback(void *id) {
    NSCParameterAssert(id != NULL);

    SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;
    return (int)decoder->_inputSource.supportsSeeking;
}

@interface SFBWavPackDecoder () {
  @private
    WavpackStreamReader64 _streamReader;
    WavpackContext *_wpc;
    int32_t *_buffer;
    AVAudioFramePosition _framePosition;
    AVAudioFramePosition _frameLength;
}
@end

@implementation SFBWavPackDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"wv"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/wavpack", @"audio/x-wavpack" ]];
}

+ (SFBAudioDecoderName)decoderName {
    return SFBAudioDecoderNameWavPack;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != NULL);

    NSData *header = [inputSource readHeaderOfLength:SFBWavPackDetectionSize skipID3v2Tag:NO error:error];
    if (!header) {
        return NO;
    }

    if ([header isWavPackHeader]) {
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else {
        *formatIsSupported = SFBTernaryTruthValueFalse;
    }

    return YES;
}

- (BOOL)decodingIsLossless {
    return (WavpackGetMode(_wpc) & MODE_LOSSLESS) == MODE_LOSSLESS;
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    _streamReader.read_bytes = read_bytes_callback;
    _streamReader.get_pos = get_pos_callback;
    _streamReader.set_pos_abs = set_pos_abs_callback;
    _streamReader.set_pos_rel = set_pos_rel_callback;
    _streamReader.push_back_byte = push_back_byte_callback;
    _streamReader.get_length = get_length_callback;
    _streamReader.can_seek = can_seek_callback;

    char errorBuf[80];

    // Setup converter
    _wpc = WavpackOpenFileInputEx64(&_streamReader, (__bridge void *)self, NULL, errorBuf,
                                    OPEN_WVC | OPEN_NORMALIZE /* | OPEN_DSD_NATIVE*/, 0);
    if (!_wpc) {
        os_log_error(gSFBAudioDecoderLog, "Error opening WavPack file: %s", errorBuf);
        if (error) {
            *error = [self invalidFormatError:NSLocalizedString(@"WavPack", @"")];
        }
        return NO;
    }

    AVAudioChannelLayout *channelLayout = nil;

    // Attempt to use the standard WAVE channel mask
    int channelMask = WavpackGetChannelMask(_wpc);
    if (channelMask) {
        AudioChannelLayout layout = {
                .mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap,
                .mChannelBitmap = channelMask,
                .mNumberChannelDescriptions = 0,
        };

        AudioChannelLayoutTag tag = 0;
        UInt32 propertySize = sizeof(tag);
        OSStatus status = AudioFormatGetProperty(kAudioFormatProperty_TagForChannelLayout, sizeof layout, &layout,
                                                 &propertySize, &tag);
        if (status == noErr) {
            channelLayout = [[AVAudioChannelLayout alloc] initWithLayoutTag:tag];
        } else {
            channelLayout = [AVAudioChannelLayout layoutWithLayout:&layout];
        }
    }

    // Fall back on the WavPack channel identities
    if (!channelLayout) {
        int channelCount = WavpackGetNumChannels(_wpc);
        unsigned char identities[channelCount + 1];
        WavpackGetChannelIdentities(_wpc, identities);

        // Convert from WavPack channel identity to Core Audio channel label
        AudioChannelLabel labels[channelCount];

        // from pack_utils.c:
        //
        // The channel IDs so far reserved are listed here:
        //
        // 0:           not allowed / terminator
        // 1 - 18:      Microsoft standard channels
        // 30, 31:      Stereo mix from RF64 (not really recommended, but RF64 specifies this)
        // 33 - 44:     Core Audio channels (see Core Audio specification)
        // 127 - 128:   Amio LeftHeight, Amio RightHeight
        // 138 - 142:   Amio BottomFrontLeft/Center/Right, Amio ProximityLeft/Right
        // 200 - 207:   Core Audio channels (see Core Audio specification)
        // 221 - 224:   Core Audio channels 301 - 305 (offset by 80)
        // 255:         Present but unknown or unused channel

        for (int i = 0; i < channelCount; ++i) {
            unsigned char ident = identities[i];
            if ((ident >= 1 && ident <= 18) || (ident >= 33 && ident <= 44) || (ident >= 200 && ident <= 207)) {
                labels[i] = ident;
            } else if (ident >= 221 && ident <= 224) {
                labels[i] = ident + 80;
            } else {
                switch (ident) {
                case 30:
                    labels[i] = kAudioChannelLabel_Left;
                    break;
                case 31:
                    labels[i] = kAudioChannelLabel_Right;
                    break;

                    // FIXME: amio mappings are approximate (or possibly incorrect)
                case 127:
                    labels[i] = kAudioChannelLabel_VerticalHeightLeft;
                    break;
                case 128:
                    labels[i] = kAudioChannelLabel_VerticalHeightRight;
                    break;
                case 138:
                    labels[i] = kAudioChannelLabel_LeftBottom;
                    break;
                case 139:
                    labels[i] = kAudioChannelLabel_CenterBottom;
                    break;
                case 140:
                    labels[i] = kAudioChannelLabel_RightBottom;
                    break;
                case 141:
                    labels[i] = kAudioChannelLabel_LeftEdgeOfScreen;
                    break;
                case 142:
                    labels[i] = kAudioChannelLabel_RightEdgeOfScreen;
                    break;

                case 255:
                    labels[i] = kAudioChannelLabel_Unknown;
                    break;

                default:
                    os_log_error(gSFBAudioDecoderLog, "Invalid WavPack channel ID: %d", identities[i]);
                    labels[i] = kAudioChannelLabel_Unused;
                    break;
                }
            }
        }

        channelLayout = [AVAudioChannelLayout layoutWithChannelLabels:labels count:channelCount];
    }

    // Floating-point and lossy files will be handed off in the canonical Core Audio format
    int mode = WavpackGetMode(_wpc);
    //    int qmode = WavpackGetQualifyMode(_wpc);
    if (MODE_FLOAT & mode || !(MODE_LOSSLESS & mode)) {
        _processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                             sampleRate:WavpackGetSampleRate(_wpc)
                                                            interleaved:NO
                                                          channelLayout:channelLayout];
        //    } else if(qmode & QMODE_DSD_AUDIO) {
    } else {
        AudioStreamBasicDescription processingStreamDescription = {0};

        processingStreamDescription.mFormatID = kAudioFormatLinearPCM;
        processingStreamDescription.mFormatFlags =
                kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;

        // Align high because Apple's AudioConverter doesn't handle low alignment
        if (WavpackGetBitsPerSample(_wpc) != 32) {
            processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
        }

        processingStreamDescription.mSampleRate = WavpackGetSampleRate(_wpc);
        processingStreamDescription.mChannelsPerFrame = (UInt32)WavpackGetNumChannels(_wpc);
        processingStreamDescription.mBitsPerChannel = (UInt32)WavpackGetBitsPerSample(_wpc);

        processingStreamDescription.mBytesPerPacket = 4;
        processingStreamDescription.mFramesPerPacket = 1;
        processingStreamDescription.mBytesPerFrame =
                processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

        _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription
                                                               channelLayout:channelLayout];
    }

    _framePosition = 0;
    _frameLength = WavpackGetNumSamples64(_wpc);

    // Set up the source format
    AudioStreamBasicDescription sourceStreamDescription = {0};

    sourceStreamDescription.mFormatID = kSFBAudioFormatWavPack;

    sourceStreamDescription.mSampleRate = WavpackGetSampleRate(_wpc);
    sourceStreamDescription.mChannelsPerFrame = (UInt32)WavpackGetNumChannels(_wpc);
    sourceStreamDescription.mBitsPerChannel = (UInt32)WavpackGetBitsPerSample(_wpc);
    sourceStreamDescription.mBytesPerPacket = (UInt32)WavpackGetBytesPerSample(_wpc);

    _sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription
                                                       channelLayout:channelLayout];

    // Populate codec properties
    _properties = @{
        SFBAudioDecodingPropertiesKeyWavPackMode : @(WavpackGetMode(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackQualifyMode : @(WavpackGetQualifyMode(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackVersion : @(WavpackGetVersion(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackFileFormat : @(WavpackGetFileFormat(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackNumberSamples : @(WavpackGetNumSamples64(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackNumberSamplesInFrame : @(WavpackGetNumSamplesInFrame(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackSampleRate : @(WavpackGetSampleRate(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackNativeSampleRate : @(WavpackGetNativeSampleRate(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackBitsPerSample : @(WavpackGetBitsPerSample(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackBytesPerSample : @(WavpackGetBytesPerSample(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackNumberChannels : @(WavpackGetNumChannels(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackChannelMask : @(WavpackGetChannelMask(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackReducedChannels : @(WavpackGetReducedChannels(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackFloatNormExponent : @(WavpackGetFloatNormExp(_wpc)),
        SFBAudioDecodingPropertiesKeyWavPackRatio : @(WavpackGetRatio(_wpc)),
    };

    _buffer = malloc(sizeof(int32_t) * (size_t)BUFFER_SIZE_FRAMES * (size_t)WavpackGetNumChannels(_wpc));
    if (!_buffer) {
        WavpackCloseFile(_wpc);
        _wpc = NULL;

        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }

        return NO;
    }

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_buffer) {
        free(_buffer);
        _buffer = NULL;
    }
    if (_wpc) {
        WavpackCloseFile(_wpc);
        _wpc = NULL;
    }

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _wpc != NULL;
}

- (AVAudioFramePosition)framePosition {
    return _framePosition;
}

- (AVAudioFramePosition)frameLength {
    return _frameLength;
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

    AVAudioFrameCount framesRemaining = frameLength;
    while (framesRemaining > 0) {
        uint32_t framesToRead = MIN(framesRemaining, BUFFER_SIZE_FRAMES);

        // Wavpack uses "complete" samples (one sample across all channels), i.e. a Core Audio frame
        uint32_t samplesRead = WavpackUnpackSamples(_wpc, _buffer, framesToRead);

        if (samplesRead == 0) {
            break;
        }

        // FIXME: What is the best way to detect a decoding error here?
        // The documentation states:
        // The actual number of samples unpacked is returned, which should be equal to the number requested unless the
        // end of file is encountered or an error occurs. If all samples have been unpacked then 0 will be returned.

        // The samples returned are handled differently based on the file's mode
        int mode = WavpackGetMode(_wpc);
        //        int qmode = WavpackGetQualifyMode(_wpc);

        // Floating point files require no special handling other than deinterleaving
        if (mode & MODE_FLOAT) {
            float *const *floatChannelData = buffer.floatChannelData;
            AVAudioChannelCount channelCount = buffer.format.channelCount;
            for (AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
                const float *input = (float *)_buffer + channel;
                float *output = floatChannelData[channel] + buffer.frameLength;
                for (uint32_t sample = 0; sample < samplesRead; ++sample) {
                    *output++ = *input;
                    input += channelCount;
                }
            }

            buffer.frameLength += samplesRead;
        }
        // Lossless files will be handed off as integers
        else if (mode & MODE_LOSSLESS) {
            // WavPack hands us 32-bit signed integers with the samples low-aligned
            int shift = 8 * (4 - WavpackGetBytesPerSample(_wpc));

            int32_t *const *int32ChannelData = buffer.int32ChannelData;
            AVAudioChannelCount channelCount = buffer.format.channelCount;

            // Deinterleave the 32-bit samples, shifting to high alignment
            if (shift) {
                for (AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
                    const int32_t *input = _buffer + channel;
                    int32_t *output = int32ChannelData[channel] + buffer.frameLength;
                    for (uint32_t sample = 0; sample < samplesRead; ++sample) {
                        *output++ = (int32_t)((uint32_t)*input << shift);
                        input += channelCount;
                    }
                }
            }
            // Just deinterleave the 32-bit samples
            else {
                for (AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
                    const int32_t *input = _buffer + channel;
                    int32_t *output = int32ChannelData[channel] + buffer.frameLength;
                    for (uint32_t sample = 0; sample < samplesRead; ++sample) {
                        *output++ = *input;
                        input += channelCount;
                    }
                }
            }

            buffer.frameLength += samplesRead;
        }
        // Convert lossy files to float
        else {
            float scaleFactor = ((uint32_t)1 << ((WavpackGetBytesPerSample(_wpc) * 8) - 1));

            // Deinterleave the 32-bit samples and convert to float
            float *const *floatChannelData = buffer.floatChannelData;
            AVAudioChannelCount channelCount = buffer.format.channelCount;
            for (AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
                const int32_t *input = _buffer + channel;
                float *output = floatChannelData[channel] + buffer.frameLength;
                for (uint32_t sample = 0; sample < samplesRead; ++sample) {
                    *output++ = *input / scaleFactor;
                    input += channelCount;
                }
            }

            buffer.frameLength += samplesRead;
        }

        framesRemaining -= samplesRead;
        _framePosition += samplesRead;
    }

    return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    NSParameterAssert(frame >= 0);

    if (!WavpackSeekSample64(_wpc, frame)) {
        os_log_error(gSFBAudioDecoderLog, "WavPack seek error");
        if (error) {
            *error = [self genericSeekError];
        }
        return NO;
    }

    _framePosition = frame;
    return YES;
}

@end
