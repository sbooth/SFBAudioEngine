//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBOggVorbisEncoder.h"

#import <vorbis/vorbisenc.h>

#import <os/log.h>

SFBAudioEncoderName const SFBAudioEncoderNameOggVorbis = @"org.sbooth.AudioEngine.Encoder.OggVorbis";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisTargetIsBitrate = @"Encoding Target is Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisQuality = @"Quality";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisBitrate = @"Nominal Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisMinBitrate = @"Minimum Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisMaxBitrate = @"Maximum Bitrate";

@interface SFBOggVorbisEncoder () {
  @private
    vorbis_info _vi;
    vorbis_dsp_state _vd;
    vorbis_block _vb;
    ogg_stream_state _os;
    BOOL _isOpen;
    AVAudioFramePosition _framePosition;
}
@end

@implementation SFBOggVorbisEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithArray:@[ @"oga", @"ogg" ]];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=vorbis"];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameOggVorbis;
}

- (BOOL)encodingIsLossless {
    return NO;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat {
    NSParameterAssert(sourceFormat != nil);

    // Validate format
    if (sourceFormat.channelCount < 1 || sourceFormat.channelCount > 8) {
        return nil;
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
    }

    if (!channelLayout) {
        return nil;
    }

    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                            sampleRate:sourceFormat.sampleRate
                                           interleaved:NO
                                         channelLayout:channelLayout];
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    // Initialize the ogg stream
    int result = ogg_stream_init(&_os, (int)arc4random());
    if (result == -1) {
        os_log_error(gSFBAudioEncoderLog, "ogg_stream_init failed");
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    // Setup the encoder
    vorbis_info_init(&_vi);

    // Encoder mode
    if ([[_settings objectForKey:SFBAudioEncodingSettingsKeyVorbisTargetIsBitrate] boolValue]) {
        NSNumber *nominal_bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyVorbisBitrate];
        NSNumber *min_bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyVorbisMinBitrate];
        NSNumber *max_bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyVorbisMaxBitrate];

        result = vorbis_encode_init(&_vi, _processingFormat.channelCount, (long)_processingFormat.sampleRate,
                                    (min_bitrate != nil) ? min_bitrate.longValue * 1000 : -1,
                                    (nominal_bitrate != nil) ? nominal_bitrate.longValue * 1000 : 128000,
                                    (max_bitrate != nil) ? max_bitrate.longValue * 1000 : -1);
        if (result != 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_encode_init failed: %d", result);
            vorbis_info_clear(&_vi);
            ogg_stream_clear(&_os);
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    } else {
        float quality_value = 0.5f;
        NSNumber *quality = [_settings objectForKey:SFBAudioEncodingSettingsKeyVorbisQuality];
        if (quality != nil) {
            quality_value = MAX(-0.1f, MIN(1.0f, quality.floatValue));
        }

        result = vorbis_encode_init_vbr(&_vi, _processingFormat.channelCount, (long)_processingFormat.sampleRate,
                                        quality_value);
        if (result != 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_encode_init_vbr failed: %d", result);
            vorbis_info_clear(&_vi);
            ogg_stream_clear(&_os);
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    vorbis_analysis_init(&_vd, &_vi);
    vorbis_block_init(&_vd, &_vb);

    vorbis_comment vc;
    vorbis_comment_init(&vc);

    ogg_packet op;
    ogg_packet op_comm;
    ogg_packet op_code;

    // Write stream headers
    result = vorbis_analysis_headerout(&_vd, &vc, &op, &op_comm, &op_code);
    if (result != 0) {
        os_log_error(gSFBAudioEncoderLog, "vorbis_encode_init failed: %d", result);
        vorbis_comment_clear(&vc);
        vorbis_block_clear(&_vb);
        vorbis_dsp_clear(&_vd);
        vorbis_info_clear(&_vi);
        ogg_stream_clear(&_os);
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    ogg_stream_packetin(&_os, &op);
    ogg_stream_packetin(&_os, &op_comm);
    ogg_stream_packetin(&_os, &op_code);

    for (;;) {
        ogg_page og;
        if (ogg_stream_flush(&_os, &og) == 0) {
            break;
        }

        NSInteger bytesWritten;
        if (![_outputTarget writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] ||
            bytesWritten != og.header_len) {
            vorbis_comment_clear(&vc);
            vorbis_block_clear(&_vb);
            vorbis_dsp_clear(&_vd);
            vorbis_info_clear(&_vi);
            ogg_stream_clear(&_os);
            return NO;
        }

        if (![_outputTarget writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] ||
            bytesWritten != og.body_len) {
            vorbis_comment_clear(&vc);
            vorbis_block_clear(&_vb);
            vorbis_dsp_clear(&_vd);
            vorbis_info_clear(&_vi);
            ogg_stream_clear(&_os);
            return NO;
        }
    }

    vorbis_comment_clear(&vc);

    AudioStreamBasicDescription outputStreamDescription = {0};
    outputStreamDescription.mFormatID = kSFBAudioFormatVorbis;
    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription
                                                       channelLayout:_processingFormat.channelLayout];

    _framePosition = 0;
    _isOpen = YES;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    vorbis_block_clear(&_vb);
    vorbis_dsp_clear(&_vd);
    vorbis_info_clear(&_vi);
    ogg_stream_clear(&_os);

    _isOpen = NO;

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _isOpen;
}

- (AVAudioFramePosition)framePosition {
    return _framePosition;
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    frameLength = MIN(frameLength, buffer.frameLength);
    if (frameLength == 0) {
        return YES;
    }

    float **buf = vorbis_analysis_buffer(&_vd, (int)frameLength);

    for (AVAudioChannelCount i = 0; i < buffer.format.channelCount; ++i) {
        memcpy(buf[i], buffer.floatChannelData[i], frameLength * buffer.format.streamDescription->mBytesPerFrame);
    }

    if (vorbis_analysis_wrote(&_vd, (int)frameLength) != 0) {
        os_log_error(gSFBAudioEncoderLog, "vorbis_analysis_wrote failed");
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    for (;;) {
        int result = vorbis_analysis_blockout(&_vd, &_vb);
        if (result == 0) {
            break;
        }
        if (result < 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_analysis_blockout failed: %d", result);
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        if (vorbis_analysis(&_vb, NULL) != 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_analysis failed");
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        if (vorbis_bitrate_addblock(&_vb) != 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_bitrate_addblock failed");
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        ogg_packet op;
        for (;;) {
            result = vorbis_bitrate_flushpacket(&_vd, &op);
            if (result == 0) {
                break;
            }
            if (result < 0) {
                os_log_error(gSFBAudioEncoderLog, "vorbis_bitrate_flushpacket failed: %d", result);
                if (error) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }

            ogg_stream_packetin(&_os, &op);

            // Write out pages (if any)
            for (;;) {
                ogg_page og;
                if (ogg_stream_pageout(&_os, &og) == 0) {
                    break;
                }

                NSInteger bytesWritten;
                if (![_outputTarget writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] ||
                    bytesWritten != og.header_len) {
                    return NO;
                }

                if (![_outputTarget writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] ||
                    bytesWritten != og.body_len) {
                    return NO;
                }

                if (ogg_page_eos(&og)) {
                    break;
                }
            }
        }
    }

    _framePosition += frameLength;

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    if (vorbis_analysis_wrote(&_vd, 0) != 0) {
        os_log_error(gSFBAudioEncoderLog, "vorbis_analysis_wrote failed");
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    for (;;) {
        int result = vorbis_analysis_blockout(&_vd, &_vb);
        if (result == 0) {
            break;
        }
        if (result < 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_analysis_blockout failed: %d", result);
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        if (vorbis_analysis(&_vb, NULL) != 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_analysis failed");
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        if (vorbis_bitrate_addblock(&_vb) != 0) {
            os_log_error(gSFBAudioEncoderLog, "vorbis_bitrate_addblock failed");
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }

        ogg_packet op;
        for (;;) {
            result = vorbis_bitrate_flushpacket(&_vd, &op);
            if (result == 0) {
                break;
            }
            if (result < 0) {
                os_log_error(gSFBAudioEncoderLog, "vorbis_bitrate_flushpacket failed: %d", result);
                if (error) {
                    *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                                 code:SFBAudioEncoderErrorCodeInternalError
                                             userInfo:nil];
                }
                return NO;
            }

            ogg_stream_packetin(&_os, &op);

            // Write out pages (if any)
            for (;;) {
                ogg_page og;
                if (ogg_stream_pageout(&_os, &og) == 0) {
                    break;
                }

                NSInteger bytesWritten;
                if (![_outputTarget writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] ||
                    bytesWritten != og.header_len) {
                    return NO;
                }

                if (![_outputTarget writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] ||
                    bytesWritten != og.body_len) {
                    return NO;
                }

                if (ogg_page_eos(&og)) {
                    break;
                }
            }
        }
    }

    return YES;
}

@end
