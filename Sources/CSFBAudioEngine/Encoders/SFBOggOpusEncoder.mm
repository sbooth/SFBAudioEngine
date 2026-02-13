//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBOggOpusEncoder.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>
#import <opus/opusenc.h>

#import <os/log.h>

#import <algorithm>
#import <memory>

SFBAudioEncoderName const SFBAudioEncoderNameOggOpus = @"org.sbooth.AudioEngine.Encoder.OggOpus";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusPreserveSampleRate = @"Preserve Sample Rate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusComplexity = @"Complexity";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrate = @"Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrateMode = @"Bitrate Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusSignalType = @"Signal Type";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusFrameDuration = @"Frame Duration";

SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeVBR = @"VBR";
SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeConstrainedVBR =
        @"Constrained VBR";
SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeHardCBR = @"Hard CBR";

SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeVoice = @"Voice";
SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeMusic = @"Music";

SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration2_5ms = @"2.5 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration5ms = @"5 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration10ms = @"10 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration20ms = @"20 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration40ms = @"40 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration60ms = @"60 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration80ms = @"80 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration100ms = @"100 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration120ms = @"120 msec";

namespace {

/// A `std::unique_ptr` deleter for `OggOpusEnc` objects
struct ogg_opus_enc_deleter {
    void operator()(OggOpusEnc *enc) { ope_encoder_destroy(enc); }
};

/// A `std::unique_ptr` deleter for `OggOpusComments` objects
struct ogg_opus_comments_deleter {
    void operator()(OggOpusComments *comments) { ope_comments_destroy(comments); }
};

using ogg_opus_enc_unique_ptr = std::unique_ptr<OggOpusEnc, ogg_opus_enc_deleter>;
using ogg_opus_comments_unique_ptr = std::unique_ptr<OggOpusComments, ogg_opus_comments_deleter>;

int writeCallback(void *user_data, const unsigned char *ptr, opus_int32 len) noexcept {
    SFBOggOpusEncoder *encoder = (__bridge SFBOggOpusEncoder *)user_data;
    NSInteger bytesWritten;
    return ![encoder->_outputTarget writeBytes:ptr length:len bytesWritten:&bytesWritten error:nil] ||
           bytesWritten != len;
}

int closeCallback(void *user_data) noexcept {
    SFBOggOpusEncoder *encoder = (__bridge SFBOggOpusEncoder *)user_data;
    return ![encoder->_outputTarget closeReturningError:nil];
}

} /* namespace */

@interface SFBOggOpusEncoder () {
  @private
    ogg_opus_enc_unique_ptr _enc;
    ogg_opus_comments_unique_ptr _comments;
    AVAudioPCMBuffer *_frameBuffer;
    AVAudioFramePosition _framePosition;
}
@end

@implementation SFBOggOpusEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"opus"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=opus"];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameOggOpus;
}

- (BOOL)encodingIsLossless {
    return NO;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat {
    NSParameterAssert(sourceFormat != nil);

    // Validate format
    if (sourceFormat.channelCount < 1 || sourceFormat.channelCount > 255) {
        return nil;
    }

    double sampleRate = 48000;
    if ([[_settings objectForKey:SFBAudioEncodingSettingsKeyOpusPreserveSampleRate] boolValue]) {
        if (sourceFormat.sampleRate < 100 || sourceFormat.sampleRate > 768000) {
            return nil;
        }
        sampleRate = sourceFormat.sampleRate;
    }

    AVAudioChannelLayout *channelLayout = nil;
    switch (sourceFormat.channelCount) {
        // Default channel layouts from Vorbis I specification section 4.3.9
        // http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
    case 1:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];
        break;
    case 2:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
        break;
    case 3:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Ogg_3_0];
        break;
    case 4:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Ogg_4_0];
        break;
    case 5:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Ogg_5_0];
        break;
    case 6:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Ogg_5_1];
        break;
    case 7:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Ogg_6_1];
        break;
    case 8:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Ogg_7_1];
        break;
    default:
        channelLayout =
                [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | sourceFormat.channelCount)];
        break;
    }

    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                            sampleRate:sampleRate
                                           interleaved:YES
                                         channelLayout:channelLayout];
}

- (BOOL)openReturningError:(NSError **)error {
    //    NSAssert(_processingFormat.sampleRate <= 768000, @"Invalid sample rate: %g", _processingFormat.sampleRate);
    //    NSAssert(_processingFormat.sampleRate >= 100, @"Invalid sample rate: %g", _processingFormat.sampleRate);
    //    NSAssert(_processingFormat.channelCount < 1, @"Invalid channel count: %d", _processingFormat.channelCount);
    //    NSAssert(_processingFormat.channelCount > 255, @"Invalid channel count: %d", _processingFormat.channelCount);

    if (![super openReturningError:error]) {
        return NO;
    }

    OpusEncCallbacks callbacks = {writeCallback, closeCallback};

    ogg_opus_comments_unique_ptr comments{ope_comments_create()};
    if (!comments) {
        os_log_error(gSFBAudioEncoderLog, "ope_comments_create failed");
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    char version[128];
    snprintf(version, 128, "SFBAudioEngine Ogg Opus Encoder (%s)", opus_get_version_string());
    int result = ope_comments_add(comments.get(), "ENCODER", version);
    if (result != OPE_OK) {
        os_log_error(gSFBAudioEncoderLog, "ope_comments_add(ENCODER) failed: %{public}s", ope_strerror(result));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    ogg_opus_enc_unique_ptr enc{(ope_encoder_create_callbacks(
            &callbacks, (__bridge void *)self, comments.get(), static_cast<opus_int32>(_processingFormat.sampleRate),
            static_cast<int>(_processingFormat.channelCount),
            _processingFormat.channelCount > 8 ? 255 : _processingFormat.channelCount > 2, &result))};
    if (!enc) {
        os_log_error(gSFBAudioEncoderLog, "ope_encoder_create_callbacks failed: %{public}s", ope_strerror(result));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    if (NSNumber *bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusBitrate]; bitrate != nil) {
        opus_int32 intValue = bitrate.intValue;
        if (intValue >= 6 && intValue <= 256) {
            // TODO: Opus now supports from 500-512000 bits per second, auto, and max
            result = ope_encoder_ctl(
                    enc.get(),
                    OPUS_SET_BITRATE(std::min(256 * static_cast<opus_int32>(_processingFormat.channelCount), intValue) *
                                     1000));
            if (result != OPE_OK) {
                os_log_error(gSFBAudioEncoderLog, "OPUS_SET_BITRATE failed: %{public}s", ope_strerror(result));
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Opus bitrate: %d", intValue);
        }
    }

    if (SFBAudioEncodingSettingsValue bitrateMode = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusBitrateMode];
        bitrateMode) {
        if (bitrateMode == SFBAudioEncodingSettingsValueOpusBitrateModeVBR) {
            result = ope_encoder_ctl(enc.get(), OPUS_SET_VBR(1));
        } else if (bitrateMode == SFBAudioEncodingSettingsValueOpusBitrateModeConstrainedVBR) {
            result = ope_encoder_ctl(enc.get(), OPUS_SET_VBR_CONSTRAINT(1));
        } else if (bitrateMode == SFBAudioEncodingSettingsValueOpusBitrateModeHardCBR) {
            result = ope_encoder_ctl(enc.get(), OPUS_SET_VBR(0));
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Opus bitrate mode: %{public}@", bitrateMode);
        }

        if (result != OPE_OK) {
            os_log_error(gSFBAudioEncoderLog, "OPUS_SET_VBR[_CONSTRAINT] failed: %{public}s", ope_strerror(result));
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    if (NSNumber *complexity = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusComplexity]; complexity != nil) {
        int intValue = complexity.intValue;
        if (intValue >= 0 && intValue <= 10) {
            result = ope_encoder_ctl(enc.get(), OPUS_SET_COMPLEXITY(intValue));
            if (result != OPE_OK) {
                os_log_error(gSFBAudioEncoderLog, "OPUS_SET_COMPLEXITY failed: %{public}s", ope_strerror(result));
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Opus complexity: %d", intValue);
        }
    }

    if (SFBAudioEncodingSettingsValue signalType = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusSignalType];
        signalType) {
        if (signalType == SFBAudioEncodingSettingsValueOpusSignalTypeVoice) {
            result = ope_encoder_ctl(enc.get(), OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
        } else if (signalType == SFBAudioEncodingSettingsValueOpusSignalTypeMusic) {
            result = ope_encoder_ctl(enc.get(), OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Opus signal type: %{public}@", signalType);
        }

        if (result != OPE_OK) {
            os_log_error(gSFBAudioEncoderLog, "OPUS_SET_SIGNAL failed: %{public}s", ope_strerror(result));
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    // Default in opusenc.c
    AVAudioFrameCount frameCapacity = 960;

    if (SFBAudioEncodingSettingsValue frameDuration =
                [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusFrameDuration];
        frameDuration) {
        if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration2_5ms) {
            frameCapacity = 120;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_2_5_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration5ms) {
            frameCapacity = 240;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_5_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration10ms) {
            frameCapacity = 480;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_10_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration20ms) {
            frameCapacity = 960;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration40ms) {
            frameCapacity = 1920;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_40_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration60ms) {
            frameCapacity = 2880;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_60_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration80ms) {
            frameCapacity = 3840;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_80_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration100ms) {
            frameCapacity = 4800;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_100_MS));
        } else if (frameDuration == SFBAudioEncodingSettingsValueOpusFrameDuration120ms) {
            frameCapacity = 5760;
            result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_120_MS));
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Opus frame duration: %{public}@", frameDuration);
        }

        if (result != OPE_OK) {
            os_log_error(gSFBAudioEncoderLog, "OPUS_SET_EXPERT_FRAME_DURATION failed: %{public}s",
                         ope_strerror(result));
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    _frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:frameCapacity];

    AudioStreamBasicDescription outputStreamDescription{};
    outputStreamDescription.mFormatID = kAudioFormatOpus;
    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription
                                                       channelLayout:_processingFormat.channelLayout];

    _enc = std::move(enc);
    _comments = std::move(comments);

    _framePosition = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _enc.reset();
    _comments.reset();
    _frameBuffer = nil;

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _enc != nullptr;
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

    // Split buffer into Opus page-sized chunks
    AVAudioFrameCount framesProcessed = 0;

    for (;;) {
        AVAudioFrameCount framesCopied = [_frameBuffer appendFromBuffer:buffer readingFromOffset:framesProcessed];
        framesProcessed += framesCopied;

        // Encode the next Opus frame
        if (_frameBuffer.isFull) {
            int result = ope_encoder_write_float(_enc.get(),
                                                 static_cast<float *>(_frameBuffer.audioBufferList->mBuffers[0].mData),
                                                 static_cast<int>(_frameBuffer.frameLength));
            if (result != OPE_OK) {
                os_log_error(gSFBAudioEncoderLog, "ope_encoder_write_float failed: %{public}s", ope_strerror(result));
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }

            _framePosition += _frameBuffer.frameLength;
            _frameBuffer.frameLength = 0;
        }

        // All complete frames were processed
        if (framesProcessed == frameLength) {
            break;
        }
    }

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    // Write remaining partial frame
    if (!_frameBuffer.isEmpty) {
        int result = ope_encoder_write_float(_enc.get(),
                                             static_cast<float *>(_frameBuffer.audioBufferList->mBuffers[0].mData),
                                             static_cast<int>(_frameBuffer.frameLength));
        if (result != OPE_OK) {
            os_log_error(gSFBAudioEncoderLog, "ope_encoder_write_float failed: %{public}s", ope_strerror(result));
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        _framePosition += _frameBuffer.frameLength;
        _frameBuffer.frameLength = 0;
    }

    int result = ope_encoder_drain(_enc.get());
    if (result != OPE_OK) {
        os_log_error(gSFBAudioEncoderLog, "ope_encoder_drain failed: %{public}s", ope_strerror(result));
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
