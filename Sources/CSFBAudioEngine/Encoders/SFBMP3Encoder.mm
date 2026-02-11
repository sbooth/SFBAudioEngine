//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBMP3Encoder.h"

#import <os/log.h>

#import <algorithm>
#import <memory>

#import <lame/lame.h>

SFBAudioEncoderName const SFBAudioEncoderNameMP3 = @"org.sbooth.AudioEngine.Encoder.MP3";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Quality = @"Quality";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3ConstantBitrate = @"Constant Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3AverageBitrate = @"Average Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3UseVariableBitrate = @"Use Variable Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRQuality = @"VBR Quality";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRMinimumBitrate = @"VBR Minimum Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRMaximumBitrate = @"VBR Maximum Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3StereoMode = @"Stereo Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3CalculateReplayGain = @"Calculate Replay Gain";

SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeMono = @"Mono";
SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeStereo = @"Stereo";
SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeJointStereo =
        @"Joint Stereo";

namespace {

/// A `std::unique_ptr` deleter for `lame_global_flags` objects
struct lame_global_flags_deleter {
    void operator()(lame_global_flags *gfp) { lame_close(gfp); }
};

using lame_global_flags_unique_ptr = std::unique_ptr<lame_global_flags, lame_global_flags_deleter>;

} /* namespace */

@interface SFBMP3Encoder () {
  @private
    lame_global_flags_unique_ptr _gfp;
    AVAudioFramePosition _framePosition;
    NSInteger _id3v2TagSize;
}
- (BOOL)flushEncoderReturningError:(NSError **)error;
- (BOOL)writeID3v1TagReturningError:(NSError **)error;
- (BOOL)writeID3v2TagReturningError:(NSError **)error;
- (BOOL)writeXingHeaderReturningError:(NSError **)error;
@end

@implementation SFBMP3Encoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"mp3"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/mpeg"];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameMP3;
}

- (BOOL)encodingIsLossless {
    return NO;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat {
    NSParameterAssert(sourceFormat != nil);

    // Validate format
    if (sourceFormat.channelCount < 1 || sourceFormat.channelCount > 2) {
        return nil;
    }

    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                            sampleRate:sourceFormat.sampleRate
                                              channels:sourceFormat.channelCount
                                           interleaved:YES];
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    lame_global_flags_unique_ptr gfp{lame_init()};
    if (!gfp) {
        os_log_error(gSFBAudioEncoderLog, "lame_init failed");
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    // Write Xing header
    lame_set_bWriteVbrTag(gfp.get(), 1);

    // Initialize the LAME encoder
    auto result = lame_set_num_channels(gfp.get(), static_cast<int>(_processingFormat.channelCount));
    if (result == -1) {
        os_log_error(gSFBAudioEncoderLog, "lame_set_num_channels(%d) failed", _processingFormat.channelCount);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    result = lame_set_in_samplerate(gfp.get(), static_cast<int>(_processingFormat.sampleRate));
    if (result == -1) {
        os_log_error(gSFBAudioEncoderLog, "lame_set_in_samplerate(%g) failed", _processingFormat.sampleRate);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    // Adjust encoder settings

    // Noise shaping and psychoacoustics
    if (NSNumber *quality = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3Quality]; quality != nil) {
        const auto qualityValue = quality.intValue;
        if (qualityValue >= 0 && qualityValue <= 9) {
            result = lame_set_quality(gfp.get(), qualityValue);
            if (result == -1) {
                os_log_error(gSFBAudioEncoderLog, "lame_set_quality(%d) failed", qualityValue);
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }
        } else {
            os_log_info(gSFBAudioEncoderLog, "Ignoring invalid LAME quality: %d", qualityValue);
        }
    }

    // Constant bitrate encoding
    NSNumber *constantBitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3ConstantBitrate];
    if (constantBitrate != nil) {
        result = lame_set_VBR(gfp.get(), vbr_off);
        if (result == -1) {
            os_log_error(gSFBAudioEncoderLog, "lame_set_VBR(vbr_off) failed");
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        result = lame_set_brate(gfp.get(), constantBitrate.intValue);
        if (result == -1) {
            os_log_error(gSFBAudioEncoderLog, "lame_set_brate(%d) failed", constantBitrate.intValue);
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    // Average bitrate encoding
    NSNumber *averageBitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3AverageBitrate];
    if (averageBitrate != nil) {
        if (constantBitrate != nil) {
            os_log_info(gSFBAudioEncoderLog, "CBR and ABR bitrates both specified; this is probably not correct");
        }

        result = lame_set_VBR(gfp.get(), vbr_abr);
        if (result == -1) {
            os_log_error(gSFBAudioEncoderLog, "lame_set_VBR(vbr_abr) failed");
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        // values larger than 8000 are bps (like Fraunhofer), so it's strange to get 320000 bps MP3 when specifying 8000
        // bps MP3
        auto meanBitrate = averageBitrate.intValue;
        if (meanBitrate >= 8000) {
            meanBitrate = (meanBitrate + 500) / 1000;
        }
        meanBitrate = std::clamp(meanBitrate, 8, 320);

        result = lame_set_VBR_mean_bitrate_kbps(gfp.get(), meanBitrate);
        if (result == -1) {
            os_log_error(gSFBAudioEncoderLog, "lame_set_VBR_mean_bitrate_kbps(%d) failed", meanBitrate);
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    // Variable bitrate encoding
    if ([[_settings objectForKey:SFBAudioEncodingSettingsKeyMP3UseVariableBitrate] boolValue]) {
        if (constantBitrate != nil) {
            os_log_info(gSFBAudioEncoderLog,
                        "VBR encoding and CBR bitrate both specified; this is probably not correct");
        }
        if (averageBitrate != nil) {
            os_log_info(gSFBAudioEncoderLog,
                        "VBR encoding and ABR bitrate both specified; this is probably not correct");
        }

        result = lame_set_VBR(gfp.get(), vbr_default);
        if (result == -1) {
            os_log_error(gSFBAudioEncoderLog, "lame_set_VBR(vbr_default) failed");
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        if (NSNumber *vbrQuality = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3VBRQuality];
            vbrQuality != nil) {
            result = lame_set_VBR_quality(gfp.get(), vbrQuality.floatValue);
            if (result == -1) {
                os_log_error(gSFBAudioEncoderLog, "lame_set_VBR_quality(%g) failed", vbrQuality.floatValue);
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }
        }

        if (NSNumber *vbrMin = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3VBRMinimumBitrate];
            vbrMin != nil) {
            result = lame_set_brate(gfp.get(), vbrMin.intValue);
            if (result == -1) {
                os_log_error(gSFBAudioEncoderLog, "lame_set_brate(%d) failed", vbrMin.intValue);
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }

            result = lame_set_VBR_min_bitrate_kbps(gfp.get(), lame_get_brate(gfp.get()));
            if (result == -1) {
                os_log_error(gSFBAudioEncoderLog, "lame_set_VBR_min_bitrate_kbps(%d) failed",
                             lame_get_brate(gfp.get()));
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }
        }

        if (NSNumber *vbrMax = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3VBRMaximumBitrate];
            vbrMax != nil) {
            auto bitrate = vbrMax.intValue * 1000;
            result = lame_set_VBR_max_bitrate_kbps(gfp.get(), bitrate);
            if (result == -1) {
                os_log_error(gSFBAudioEncoderLog, "lame_set_VBR_max_bitrate_kbps(%d) failed", bitrate);
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }
        }
    }

    if (SFBAudioEncodingSettingsValue stereoMode = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3StereoMode];
        stereoMode != nil) {
        if (stereoMode == SFBAudioEncodingSettingsValueMP3StereoModeMono) {
            result = lame_set_mode(gfp.get(), MONO);
        } else if (stereoMode == SFBAudioEncodingSettingsValueMP3StereoModeStereo) {
            result = lame_set_mode(gfp.get(), STEREO);
        } else if (stereoMode == SFBAudioEncodingSettingsValueMP3StereoModeJointStereo) {
            result = lame_set_mode(gfp.get(), JOINT_STEREO);
        } else {
            os_log_info(gSFBAudioEncoderLog, "Ignoring unknown LAME stereo mode: %{public}@", stereoMode);
        }

        if (result == -1) {
            os_log_error(gSFBAudioEncoderLog, "lame_set_mode failed");
            if (error != nullptr) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    const auto calculateReplayGain =
            [[_settings objectForKey:SFBAudioEncodingSettingsKeyMP3CalculateReplayGain] boolValue];
    result = lame_set_findReplayGain(gfp.get(), calculateReplayGain);
    if (result == -1) {
        os_log_error(gSFBAudioEncoderLog, "lame_set_findReplayGain(%d) failed", calculateReplayGain);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    lame_set_write_id3tag_automatic(gfp.get(), false);

    result = lame_init_params(gfp.get());
    if (result == -1) {
        os_log_error(gSFBAudioEncoderLog, "lame_init_params failed");
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    if (![self writeID3v2TagReturningError:error]) {
        return NO;
    }

    AudioStreamBasicDescription outputStreamDescription{};
    outputStreamDescription.mFormatID = kAudioFormatMPEGLayer3;
    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

    _gfp = std::move(gfp);

    _framePosition = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _gfp.reset();

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _gfp != nullptr;
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

    const size_t bufsize = static_cast<size_t>(1.25 * (_processingFormat.channelCount * frameLength)) + 7200;
    auto buf = std::make_unique<unsigned char[]>(bufsize);
    if (!buf) {
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    auto result = lame_encode_buffer_interleaved_ieee_float(
            _gfp.get(), static_cast<const float *>(buffer.audioBufferList->mBuffers[0].mData),
            static_cast<int>(frameLength), buf.get(), static_cast<int>(bufsize));
    if (result == -1) {
        os_log_error(gSFBAudioEncoderLog, "lame_encode_buffer_interleaved_ieee_float failed");
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    NSInteger bytesWritten;
    if (![_outputTarget writeBytes:buf.get() length:result bytesWritten:&bytesWritten error:error] ||
        bytesWritten != result) {
        return NO;
    }

    _framePosition += frameLength;

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    if (![self flushEncoderReturningError:error]) {
        return NO;
    }

    if (![self writeID3v1TagReturningError:error]) {
        return NO;
    }

    if (![self writeXingHeaderReturningError:error]) {
        return NO;
    }

    return YES;
}

- (BOOL)flushEncoderReturningError:(NSError **)error {
    const size_t bufsize = 7200;
    auto buf = std::make_unique<unsigned char[]>(bufsize);
    if (!buf) {
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    auto result = lame_encode_flush(_gfp.get(), buf.get(), bufsize);
    if (result == -1) {
        os_log_error(gSFBAudioEncoderLog, "lame_encode_flush failed");
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    return YES;
}

- (BOOL)writeID3v1TagReturningError:(NSError **)error {
    auto bufsize = lame_get_id3v1_tag(_gfp.get(), nullptr, 0);
    if (bufsize > 0) {
        auto buf = std::make_unique<unsigned char[]>(bufsize);
        if (!buf) {
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }

        auto result = lame_get_id3v1_tag(_gfp.get(), buf.get(), bufsize);

        NSInteger bytesWritten;
        if (![_outputTarget writeBytes:buf.get()
                                length:static_cast<NSInteger>(result)
                          bytesWritten:&bytesWritten
                                 error:error] ||
            bytesWritten != static_cast<NSInteger>(result)) {
            return NO;
        }
    }

    return YES;
}

- (BOOL)writeID3v2TagReturningError:(NSError **)error {
    auto bufsize = lame_get_id3v2_tag(_gfp.get(), nullptr, 0);
    if (bufsize > 0) {
        auto buf = std::make_unique<unsigned char[]>(bufsize);
        if (!buf) {
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }

        auto result = lame_get_id3v2_tag(_gfp.get(), buf.get(), bufsize);

        NSInteger bytesWritten;
        if (![_outputTarget writeBytes:buf.get()
                                length:static_cast<NSInteger>(result)
                          bytesWritten:&bytesWritten
                                 error:error] ||
            bytesWritten != static_cast<NSInteger>(result)) {
            return NO;
        }
    }

    _id3v2TagSize = static_cast<NSInteger>(bufsize);

    return YES;
}

- (BOOL)writeXingHeaderReturningError:(NSError **)error {
    auto bufsize = lame_get_lametag_frame(_gfp.get(), nullptr, 0);
    if (bufsize > 0) {
        auto buf = std::make_unique<unsigned char[]>(bufsize);
        if (!buf) {
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }

        auto result = lame_get_lametag_frame(_gfp.get(), buf.get(), bufsize);

        if (![_outputTarget seekToOffset:_id3v2TagSize error:error]) {
            return NO;
        }

        NSInteger bytesWritten;
        if (![_outputTarget writeBytes:buf.get()
                                length:static_cast<NSInteger>(result)
                          bytesWritten:&bytesWritten
                                 error:error] ||
            bytesWritten != static_cast<NSInteger>(result)) {
            return NO;
        }
    }

    return YES;
}

@end
