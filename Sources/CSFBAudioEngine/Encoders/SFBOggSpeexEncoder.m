//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBOggSpeexEncoder.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>
#import <ogg/ogg.h>
#import <speex/speex.h>
#import <speex/speex_header.h>
#import <speex/speex_preprocess.h>
#import <speex/speex_stereo.h>
#import <speex/speexdsp_types.h>

#import <os/log.h>

SFBAudioEncoderName const SFBAudioEncoderNameOggSpeex = @"org.sbooth.AudioEngine.Encoder.OggSpeex";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexMode = @"Encoding Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexTargetIsBitrate = @"Encoding Target is Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexQuality = @"Quality";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexComplexity = @"Complexity";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexBitrate = @"Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableVBR = @"Enable VBR";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexVBRMaxBitrate = @"VBR Maximum Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableVAD = @"Enable VAD";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableDTX = @"Enable DTX";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableABR = @"Enable ABR";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexDenoiseInput = @"Denoise Input";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableAGC = @"Enable AGC";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexDisableHighpassFilter = @"Disable Highpass Filter";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexFramesPerOggPacket = @"Speex Frames per Ogg Packet";

SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeNarrowband = @"Narrowband";
SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeWideband = @"Wideband";
SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeUltraWideband = @"Ultra Wideband";

static void vorbis_comment_init(char **comments, size_t *length, const char *vendor_string) {
    size_t vendor_length = strlen(vendor_string);
    size_t len = 4 + vendor_length + 4;
    char *p = (char *)malloc(len);
    if (p == NULL) {
        *comments = NULL;
        *length = 0;
        return;
    }

    uint32_t user_comment_list_length = 0;

    OSWriteLittleInt32(p, 0, vendor_length);
    memcpy(p + 4, vendor_string, vendor_length);
    OSWriteLittleInt32(p, 4 + vendor_length, user_comment_list_length);

    *length = len;
    *comments = p;
}

#if 0
static void vorbis_comment_add(char **comments, size_t *length, const char *tag, const char *val)
{
    char *p = *comments;

    size_t vendor_length = OSReadLittleInt32(p, 0);
    size_t user_comment_list_length = OSReadLittleInt32(p, 4 + vendor_length);

    size_t tag_len = (tag ? strlen(tag) : 0);
    size_t val_len = strlen(val);
    size_t len = (*length) + 4 + tag_len + val_len;
    p = (char *)reallocf(p, len);
    if(p == NULL) {
        *comments = NULL;
        *length = 0;
        return;
    }

    OSWriteLittleInt32(p, *length, tag_len + val_len);  /* length of comment */
    if(tag) memcpy(p + *length + 4, tag, tag_len);  /* comment */
    memcpy(p + *length + 4 + tag_len, val, val_len);  /* comment */
    OSWriteLittleInt32(p, 4 + vendor_length, user_comment_list_length + 1);

    *comments = p;
    *length = len;
}
#endif

#define MAX_FRAME_BYTES 2000

@interface SFBOggSpeexEncoder () {
  @private
    ogg_stream_state _os;
    void *_st;
    SpeexPreprocessState *_preprocess;
    SpeexBits _bits;
    AVAudioPCMBuffer *_frameBuffer;
    AVAudioFramePosition _framePosition;
    spx_int32_t _speex_frame_size;
    spx_int32_t _speex_lookahead;
    spx_int32_t _speex_frames_per_ogg_packet;
    ogg_int64_t _speex_frame_number;
}
- (BOOL)encodeSpeexFrameReturningError:(NSError **)error;
@end

@implementation SFBOggSpeexEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"spx"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=speex"];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameOggSpeex;
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

    double sampleRate = sourceFormat.sampleRate;

    SFBAudioEncodingSettingsValue mode = [_settings objectForKey:SFBAudioEncodingSettingsKeySpeexMode];
    if (mode) {
        // Determine the desired sample rate
        if (mode == SFBAudioEncodingSettingsValueSpeexModeNarrowband) {
            sampleRate = 8000;
        } else if (mode == SFBAudioEncodingSettingsValueSpeexModeWideband) {
            sampleRate = 16000;
        } else if (mode == SFBAudioEncodingSettingsValueSpeexModeUltraWideband) {
            sampleRate = 32000;
        } else {
            return nil;
        }
    } else if (sampleRate > 48000 || sampleRate < 6000) {
        return nil;
    }

    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                            sampleRate:sampleRate
                                              channels:sourceFormat.channelCount
                                           interleaved:YES];
}

- (BOOL)openReturningError:(NSError **)error {
    //    NSAssert(_processingFormat.sampleRate <= 48000, @"Invalid sample rate: %g", _processingFormat.sampleRate);
    //    NSAssert(_processingFormat.sampleRate >= 6000, @"Invalid sample rate: %g", _processingFormat.sampleRate);
    //    NSAssert(_processingFormat.channelCount < 1, @"Invalid channel count: %d", _processingFormat.channelCount);
    //    NSAssert(_processingFormat.channelCount > 2, @"Invalid channel count: %d", _processingFormat.channelCount);

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
    const SpeexMode *speex_mode = NULL;
    SFBAudioEncodingSettingsValue mode = [_settings objectForKey:SFBAudioEncodingSettingsKeySpeexMode];
    if (!mode) {
        if (_processingFormat.sampleRate > 25000) {
            speex_mode = speex_lib_get_mode(SPEEX_MODEID_UWB);
        } else if (_processingFormat.sampleRate > 12500) {
            speex_mode = speex_lib_get_mode(SPEEX_MODEID_WB);
        } else if (_processingFormat.sampleRate >= 6000) {
            speex_mode = speex_lib_get_mode(SPEEX_MODEID_NB);
        }
    } else {
        if (mode == SFBAudioEncodingSettingsValueSpeexModeNarrowband) {
            speex_mode = speex_lib_get_mode(SPEEX_MODEID_NB);
        } else if (mode == SFBAudioEncodingSettingsValueSpeexModeWideband) {
            speex_mode = speex_lib_get_mode(SPEEX_MODEID_WB);
        } else if (mode == SFBAudioEncodingSettingsValueSpeexModeUltraWideband) {
            speex_mode = speex_lib_get_mode(SPEEX_MODEID_UWB);
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Speex mode: %{public}@", mode);
            ogg_stream_clear(&_os);
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    // Setup the encoder
    _st = speex_encoder_init(speex_mode);
    if (_st == NULL) {
        os_log_error(gSFBAudioEncoderLog, "Unrecognized Speex mode: %{public}@", mode);
        ogg_stream_clear(&_os);
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    speex_encoder_ctl(_st, SPEEX_GET_FRAME_SIZE, &_speex_frame_size);

    _frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat
                                                 frameCapacity:(AVAudioFrameCount)_speex_frame_size];

    NSNumber *complexity = [_settings objectForKey:SFBAudioEncodingSettingsKeySpeexComplexity];
    if (complexity == nil) {
        complexity = @3;
    }
    spx_int32_t complexity_value = complexity.intValue;
    speex_encoder_ctl(_st, SPEEX_SET_COMPLEXITY, &complexity_value);

    spx_int32_t rate = (spx_int32_t)_processingFormat.sampleRate; // 8, 16, 32
    speex_encoder_ctl(_st, SPEEX_SET_SAMPLING_RATE, &rate);

    spx_int32_t vbr_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexEnableVBR] boolValue];
    spx_int32_t vad_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexEnableVAD] boolValue];
    spx_int32_t dtx_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexEnableDTX] boolValue];
    spx_int32_t abr_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexEnableABR] boolValue];

    NSNumber *quality = [_settings objectForKey:SFBAudioEncodingSettingsKeySpeexQuality];
    if (quality == nil) {
        quality = @-1;
    }

    // Encoder mode
    if ([[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexTargetIsBitrate] boolValue]) {
        NSNumber *bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeySpeexBitrate];
        if (bitrate != nil) {
            spx_int32_t bitrate_value = bitrate.intValue;
            speex_encoder_ctl(_st, SPEEX_SET_BITRATE, &bitrate_value);
        } else {
            os_log_info(gSFBAudioEncoderLog, "Speex encoding target is bitrate but no bitrate specified");
        }
    } else if (quality.intValue >= 0) {
        spx_int32_t vbr_max = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexVBRMaxBitrate] intValue];
        if (vbr_enabled) {
            if (vbr_max > 0) {
                speex_encoder_ctl(_st, SPEEX_SET_VBR_MAX_BITRATE, &vbr_max);
            }
            float vbr_quality = quality.floatValue;
            speex_encoder_ctl(_st, SPEEX_SET_VBR_QUALITY, &vbr_quality);
        } else {
            spx_int32_t quality_value = quality.intValue;
            speex_encoder_ctl(_st, SPEEX_SET_QUALITY, &quality_value);
        }
    }

    if (vbr_enabled) {
        speex_encoder_ctl(_st, SPEEX_SET_VBR, &vbr_enabled);
    } else if (vad_enabled) {
        speex_encoder_ctl(_st, SPEEX_SET_VAD, &vad_enabled);
    }

    if (dtx_enabled) {
        speex_encoder_ctl(_st, SPEEX_SET_DTX, &dtx_enabled);
    }

    if (abr_enabled) {
        speex_encoder_ctl(_st, SPEEX_SET_ABR, &abr_enabled);
    }

    if (dtx_enabled && !(vbr_enabled || abr_enabled || vad_enabled)) {
        os_log_info(gSFBAudioEncoderLog, "DTX requires VAD, VBR, or ABR");
    } else if ((vbr_enabled || abr_enabled) && (vad_enabled)) {
        os_log_info(gSFBAudioEncoderLog, "VAD is implied by VBR or ABR");
    }

    spx_int32_t highpass_enabled =
            ![[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexDisableHighpassFilter] boolValue];
    speex_encoder_ctl(_st, SPEEX_SET_HIGHPASS, &highpass_enabled);

    speex_encoder_ctl(_st, SPEEX_GET_LOOKAHEAD, &_speex_lookahead);

    spx_int32_t denoise_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexDenoiseInput] boolValue];
    spx_int32_t agc_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexEnableAGC] boolValue];
    if (denoise_enabled || agc_enabled) {
        _preprocess = speex_preprocess_state_init(_speex_frame_size, rate);
        speex_preprocess_ctl(_preprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoise_enabled);
        speex_preprocess_ctl(_preprocess, SPEEX_PREPROCESS_SET_AGC, &agc_enabled);
        _speex_lookahead += _speex_frame_size;
    }

    // Write stream headers
    SpeexHeader header;
    speex_init_header(&header, (int)_processingFormat.sampleRate, (int)_processingFormat.channelCount, speex_mode);

    _speex_frames_per_ogg_packet = 1; // 1-10 default 1
    NSNumber *framesPerPacket = [_settings objectForKey:SFBAudioEncodingSettingsKeySpeexFramesPerOggPacket];
    if (framesPerPacket != nil) {
        int intValue = framesPerPacket.intValue;
        if (intValue < 1 || intValue > 10) {
            os_log_error(gSFBAudioEncoderLog, "Invalid Speex frames per packet: %d", intValue);
            ogg_stream_clear(&_os);
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
        _speex_frames_per_ogg_packet = intValue;
    }

    header.frames_per_packet = _speex_frames_per_ogg_packet;
    header.vbr = [[_settings objectForKey:SFBAudioEncodingSettingsKeySpeexEnableVBR] boolValue];
    header.nb_channels = (spx_int32_t)_processingFormat.channelCount;

    int packet_size;
    unsigned char *packet_data = (unsigned char *)speex_header_to_packet(&header, &packet_size);

    ogg_packet op;
    op.packet = packet_data;
    op.bytes = packet_size;
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;

    ogg_stream_packetin(&_os, &op);
    speex_header_free(packet_data);

    for (;;) {
        ogg_page og;
        if (ogg_stream_pageout(&_os, &og) == 0) {
            break;
        }

        NSInteger bytesWritten;
        if (![_outputTarget writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] ||
            bytesWritten != og.header_len) {
            speex_encoder_destroy(_st);
            ogg_stream_clear(&_os);
            return NO;
        }

        if (![_outputTarget writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] ||
            bytesWritten != og.body_len) {
            speex_encoder_destroy(_st);
            ogg_stream_clear(&_os);
            return NO;
        }

        if (ogg_page_eos(&og)) {
            break;
        }
    }

    const char *speex_version;
    char vendor_string[64];
    speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void *)&speex_version);
    snprintf(vendor_string, sizeof(vendor_string), "Encoded with Speex %s", speex_version);

    char *comments;
    size_t comments_length;
    vorbis_comment_init(&comments, &comments_length, vendor_string);

    op.packet = (unsigned char *)comments;
    op.bytes = (long)comments_length;
    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 1;

    ogg_stream_packetin(&_os, &op);
    free(comments);

    for (;;) {
        ogg_page og;
        if (ogg_stream_pageout(&_os, &og) == 0) {
            break;
        }

        NSInteger bytesWritten;
        if (![_outputTarget writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] ||
            bytesWritten != og.header_len) {
            speex_encoder_destroy(_st);
            ogg_stream_clear(&_os);
            return NO;
        }

        if (![_outputTarget writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] ||
            bytesWritten != og.body_len) {
            speex_encoder_destroy(_st);
            ogg_stream_clear(&_os);
            return NO;
        }

        if (ogg_page_eos(&og)) {
            break;
        }
    }

    speex_bits_init(&_bits);

    _speex_frame_number = -1;

    AudioStreamBasicDescription outputStreamDescription = {0};
    outputStreamDescription.mFormatID = kSFBAudioFormatSpeex;
    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

    _framePosition = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    speex_bits_destroy(&_bits);
    memset(&_bits, 0, sizeof(SpeexBits));
    if (_st) {
        speex_encoder_destroy(_st);
        _st = NULL;
    }
    if (_preprocess) {
        speex_preprocess_state_destroy(_preprocess);
        _preprocess = NULL;
    }
    ogg_stream_clear(&_os);

    _frameBuffer = nil;

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _st != NULL;
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

    // Split buffer into Speex frame-sized chunks
    AVAudioFrameCount framesProcessed = 0;

    for (;;) {
        AVAudioFrameCount framesCopied = [_frameBuffer appendFromBuffer:buffer readingFromOffset:framesProcessed];
        framesProcessed += framesCopied;

        // Encode the next Speex frame
        if (_frameBuffer.isFull) {
            if (![self encodeSpeexFrameReturningError:error]) {
                return NO;
            }
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
    // Encode any remaining audio
    if (!_frameBuffer.isEmpty && ![self encodeSpeexFrameReturningError:error]) {
        return NO;
    }
    _frameBuffer.frameLength = 0;

    char cbits[MAX_FRAME_BYTES];

    // Finish up
    if (((_speex_frame_number + 1) % _speex_frames_per_ogg_packet) != 0) {
        while (((_speex_frame_number + 1) % _speex_frames_per_ogg_packet) != 0) {
            ++_speex_frame_number;
            speex_bits_pack(&_bits, 15, 5);
        }

        int byte_count = speex_bits_write(&_bits, cbits, MAX_FRAME_BYTES);

        ogg_packet op;
        op.packet = (unsigned char *)cbits;
        op.bytes = byte_count;
        op.b_o_s = 0;
        op.e_o_s = 1;
        op.granulepos = ((_speex_frame_number + 1) * _speex_frame_size) - _speex_lookahead;
        if (op.granulepos > _framePosition) {
            op.granulepos = _framePosition;
        }

        op.packetno = 2 + (_speex_frame_number / _speex_frames_per_ogg_packet);
        ogg_stream_packetin(&_os, &op);
    }

    // Flush all pages left to be written
    for (;;) {
        ogg_page og;
        if (ogg_stream_flush(&_os, &og) == 0) {
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
    }

    return YES;
}

- (BOOL)encodeSpeexFrameReturningError:(NSError **)error {
    AVAudioFrameCount framesOfSilenceAdded = 0;
    if (!_frameBuffer.isFull) {
        framesOfSilenceAdded = [_frameBuffer fillRemainderWithSilence];
    }

    if (_processingFormat.channelCount == 2) {
        speex_encode_stereo_int(_frameBuffer.audioBufferList->mBuffers[0].mData, (int)_frameBuffer.frameLength, &_bits);
    }
    if (_preprocess) {
        speex_preprocess(_preprocess, _frameBuffer.audioBufferList->mBuffers[0].mData, NULL);
    }
    speex_encode_int(_st, _frameBuffer.audioBufferList->mBuffers[0].mData, &_bits);

    _framePosition += _frameBuffer.frameLength - framesOfSilenceAdded;

    ++_speex_frame_number;

    // Emit ogg packet
    if (((_speex_frame_number + 1) % _speex_frames_per_ogg_packet) == 0) {
        char cbits[MAX_FRAME_BYTES];

        speex_bits_insert_terminator(&_bits);
        int byte_count = speex_bits_write(&_bits, cbits, MAX_FRAME_BYTES);
        speex_bits_reset(&_bits);

        ogg_packet op;
        op.packet = (unsigned char *)cbits;
        op.bytes = byte_count;
        op.b_o_s = 0;
        op.e_o_s = (framesOfSilenceAdded > 0);
        op.granulepos = ((_speex_frame_number + 1) * _speex_frame_size) - _speex_lookahead;
        if (op.granulepos > _framePosition) {
            op.granulepos = _framePosition;
        }

        op.packetno = 2 + (_speex_frame_number / _speex_frames_per_ogg_packet);

        ogg_stream_packetin(&_os, &op);

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

    return YES;
}

@end
