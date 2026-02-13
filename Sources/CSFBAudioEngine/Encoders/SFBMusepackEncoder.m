//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBMusepackEncoder.h"

#import <mpc/libmpcenc.h>
#import <mpc/stream_encoder.h>

#import <os/log.h>

SFBAudioEncoderName const SFBAudioEncoderNameMusepack = @"org.sbooth.AudioEngine.Encoder.Musepack";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMusepackQuality = @"Quality";

static size_t my_mpc_write_callback(const void *restrict ptr, size_t size, size_t nitems, void *context) {
    NSCParameterAssert(context != NULL);
    SFBMusepackEncoder *encoder = (__bridge SFBMusepackEncoder *)context;

    NSInteger bytesWritten;
    if (![encoder->_outputTarget writeBytes:ptr
                                     length:(NSInteger)(size * nitems)
                               bytesWritten:&bytesWritten
                                      error:nil]) {
        return 0;
    }
    return (size_t)bytesWritten / size;
}

static int my_mpc_seek_callback(void *context, off_t offset, int whence) {
    NSCParameterAssert(context != NULL);
    SFBMusepackEncoder *encoder = (__bridge SFBMusepackEncoder *)context;

    switch (whence) {
    case SEEK_SET:
        // offset remains unchanged
        break;
    case SEEK_CUR: {
        NSInteger outputTargetOffset;
        if ([encoder->_outputTarget getOffset:&outputTargetOffset error:nil]) {
            offset += outputTargetOffset;
        }
        break;
    }
    case SEEK_END: {
        NSInteger outputTargetLength;
        if ([encoder->_outputTarget getLength:&outputTargetLength error:nil]) {
            offset += outputTargetLength;
        }
        break;
    }
    }

    if (![encoder->_outputTarget seekToOffset:offset error:nil]) {
        return -1;
    }

    NSInteger outputTargetOffset;
    if (![encoder->_outputTarget getOffset:&outputTargetOffset error:nil]) {
        return -1;
    }

    return 0;
}

static off_t my_mpc_tell_callback(void *context) {
    NSCParameterAssert(context != NULL);
    SFBMusepackEncoder *encoder = (__bridge SFBMusepackEncoder *)context;

    NSInteger offset;
    if (![encoder->_outputTarget getOffset:&offset error:nil]) {
        return -1;
    }

    return offset;
}

@interface SFBMusepackEncoder () {
  @private
    mpc_stream_encoder *_enc;
    AVAudioFramePosition _framePosition;
}
@end

@implementation SFBMusepackEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"mpc"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/musepack", @"audio/x-musepack" ]];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameMusepack;
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

    if (sourceFormat.sampleRate != 44100 && sourceFormat.sampleRate != 48000 && sourceFormat.sampleRate != 37800 &&
        sourceFormat.sampleRate != 32000) {
        return nil;
    }

    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                            sampleRate:sourceFormat.sampleRate
                                              channels:sourceFormat.channelCount
                                           interleaved:YES];
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    _enc = mpc_stream_encoder_create();
    if (!_enc) {
        os_log_error(gSFBAudioEncoderLog, "mpc_stream_encoder_create() failed");
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    if (_estimatedFramesToEncode > 0) {
        if (mpc_stream_encoder_set_estimated_total_frames(_enc, (mpc_uint64_t)_estimatedFramesToEncode) !=
            MPC_STATUS_OK) {
            os_log_error(gSFBAudioEncoderLog, "mpc_stream_encoder_set_estimated_total_frames failed");
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    NSNumber *quality = [_settings objectForKey:SFBAudioEncodingSettingsKeyMusepackQuality];
    if (quality != nil) {
        float quality_value = quality.floatValue;
        if (quality_value < 0 || quality_value > 10) {
            os_log_info(gSFBAudioEncoderLog, "Ignoring invalid Musepack quality: %g", quality_value);
        } else if (mpc_stream_encoder_set_quality(_enc, quality_value) != MPC_STATUS_OK) {
            os_log_error(gSFBAudioEncoderLog, "mpc_stream_encoder_set_quality failed");
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInternalError
                                         userInfo:nil];
            }
            return NO;
        }
    }

    if (mpc_stream_encoder_init(_enc, (float)_processingFormat.sampleRate, (int)_processingFormat.channelCount,
                                my_mpc_write_callback, my_mpc_seek_callback, my_mpc_tell_callback,
                                (__bridge void *)self) != MPC_STATUS_OK) {
        os_log_error(gSFBAudioEncoderLog, "mpc_stream_encoder_init failed");
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    AudioStreamBasicDescription outputStreamDescription = {0};
    outputStreamDescription.mFormatID = kSFBAudioFormatMusepack;
    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

    _framePosition = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_enc) {
        mpc_stream_encoder_destroy(_enc);
        _enc = NULL;
    }

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _enc != NULL;
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

    if (mpc_stream_encoder_encode(_enc, (const mpc_int16_t *)buffer.audioBufferList->mBuffers[0].mData, frameLength) !=
        MPC_STATUS_OK) {
        os_log_error(gSFBAudioEncoderLog, "mpc_stream_encoder_encode failed");
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    _framePosition += frameLength;

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    if (mpc_stream_encoder_finish(_enc) != MPC_STATUS_OK) {
        os_log_error(gSFBAudioEncoderLog, "mpc_stream_encoder_finish failed");
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    return YES;
}

@end
