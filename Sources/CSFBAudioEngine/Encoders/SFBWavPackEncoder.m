//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBWavPackEncoder.h"

#import "SFBCStringForOSType.h"

#import <wavpack/wavpack.h>

#import <AudioToolbox/AudioToolbox.h>
#import <CommonCrypto/CommonCrypto.h>

#import <os/log.h>

SFBAudioEncoderName const SFBAudioEncoderNameWavPack = @"org.sbooth.AudioEngine.Encoder.WavPack";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyWavPackCompressionLevel = @"Compression Level";

SFBAudioEncodingSettingsValueWavPackCompressionLevel const SFBAudioEncodingSettingsValueWavPackCompressionLevelFast =
        @"Fast";
SFBAudioEncodingSettingsValueWavPackCompressionLevel const SFBAudioEncodingSettingsValueWavPackCompressionLevelHigh =
        @"High";
SFBAudioEncodingSettingsValueWavPackCompressionLevel const
        SFBAudioEncodingSettingsValueWavPackCompressionLevelVeryHigh = @"Very High";

@interface SFBWavPackEncoder () {
  @package
    NSMutableData *_firstBlock;
  @private
    WavpackContext *_wpc;
    WavpackConfig _config;
    CC_MD5_CTX _md5;
    AVAudioFramePosition _framePosition;
}
@end

static int wavpack_block_output(void *id, void *data, int32_t bcount) {
    NSCParameterAssert(id != NULL);
    SFBWavPackEncoder *encoder = (__bridge SFBWavPackEncoder *)id;

    if (!encoder->_firstBlock) {
        encoder->_firstBlock = [NSMutableData dataWithBytes:data length:(NSUInteger)bcount];
    }

    NSInteger bytesWritten;
    return [encoder->_outputTarget writeBytes:data length:bcount bytesWritten:&bytesWritten error:nil];
}

@implementation SFBWavPackEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"wv"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/wavpack", @"audio/x-wavpack" ]];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameWavPack;
}

- (BOOL)encodingIsLossless {
    return YES;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat {
    NSParameterAssert(sourceFormat != nil);

    // Validate format
    if ((sourceFormat.streamDescription->mFormatFlags & kAudioFormatFlagIsFloat) == kAudioFormatFlagIsFloat ||
        sourceFormat.channelCount < 1 || sourceFormat.channelCount > 32) {
        return nil;
    }

    // Set up the processing format
    AudioStreamBasicDescription streamDescription = {0};

    streamDescription.mFormatID = kAudioFormatLinearPCM;
    streamDescription.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

    streamDescription.mSampleRate = sourceFormat.sampleRate;
    streamDescription.mChannelsPerFrame = sourceFormat.channelCount;
    streamDescription.mBitsPerChannel = sourceFormat.streamDescription->mBitsPerChannel;

    if (streamDescription.mBitsPerChannel == 32) {
        streamDescription.mFormatID |= kAudioFormatFlagIsPacked;
    }

    streamDescription.mBytesPerPacket = 4 * streamDescription.mChannelsPerFrame;
    streamDescription.mFramesPerPacket = 1;
    streamDescription.mBytesPerFrame = streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

    // Use WAVFORMATEX channel order
    AVAudioChannelLayout *channelLayout = nil;

    if (sourceFormat.channelLayout) {
        AudioChannelBitmap channelBitmap = 0;
        UInt32 propertySize = sizeof(channelBitmap);
        AudioChannelLayoutTag layoutTag = sourceFormat.channelLayout.layoutTag;
        OSStatus status = AudioFormatGetProperty(kAudioFormatProperty_BitmapForLayoutTag, sizeof layoutTag, &layoutTag,
                                                 &propertySize, &channelBitmap);
        if (status == noErr) {
            AudioChannelLayout acl = {.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap,
                                      .mChannelBitmap = channelBitmap,
                                      .mNumberChannelDescriptions = 0};
            channelLayout = [[AVAudioChannelLayout alloc] initWithLayout:&acl];
        }
        // TODO: Use WavPack channel identities as a fallback?
        else {
            os_log_info(gSFBAudioEncoderLog,
                        "AudioFormatGetProperty(kAudioFormatProperty_BitmapForLayoutTag), layoutTag = %d failed: %d "
                        "'%{public}.4s'",
                        layoutTag, status, SFBCStringForOSType(status));
        }
    }

    return [[AVAudioFormat alloc] initWithStreamDescription:&streamDescription channelLayout:channelLayout];
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    _wpc = WavpackOpenFileOutput(wavpack_block_output, (__bridge void *)self, NULL);
    if (!_wpc) {
        os_log_error(gSFBAudioEncoderLog, "WavpackOpenFileOutput failed");
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    //    WavpackSetFileInformation(_wpc, NULL, WP_FORMAT_WAV);

    _config.sample_rate = (int)_processingFormat.sampleRate;
    _config.num_channels = (int)_processingFormat.channelCount;
    _config.bits_per_sample = (int)_processingFormat.streamDescription->mBitsPerChannel;
    _config.bytes_per_sample = (_config.bits_per_sample + 7) / 8;

    AVAudioChannelLayout *layout = _processingFormat.channelLayout;
    if (layout) {
        _config.channel_mask = (int)layout.layout->mChannelBitmap;
    } else {
        switch (_processingFormat.channelCount) {
        case 1:
            _config.channel_mask = kAudioChannelBit_Left;
            break;
        case 2:
            _config.channel_mask = kAudioChannelBit_Left | kAudioChannelBit_Right;
            break;
        }
    }

    _config.flags = CONFIG_MD5_CHECKSUM;

    SFBAudioEncodingSettingsValue level = [_settings objectForKey:SFBAudioEncodingSettingsKeyWavPackCompressionLevel];
    if (level) {
        if (level == SFBAudioEncodingSettingsValueWavPackCompressionLevelFast) {
            _config.flags |= CONFIG_FAST_FLAG;
        } else if (level == SFBAudioEncodingSettingsValueWavPackCompressionLevelHigh) {
            _config.flags |= CONFIG_HIGH_FLAG;
        } else if (level == SFBAudioEncodingSettingsValueWavPackCompressionLevelVeryHigh) {
            _config.flags |= CONFIG_VERY_HIGH_FLAG;
        } else {
            os_log_info(gSFBAudioEncoderLog, "Ignoring unknown WavPack compression level: %{public}@", level);
        }
    }

    if (!WavpackSetConfiguration64(_wpc, &_config, _estimatedFramesToEncode > 0 ? _estimatedFramesToEncode : -1,
                                   NULL)) {
        os_log_error(gSFBAudioEncoderLog, "WavpackOpenFileOutput failed: %{public}s", WavpackGetErrorMessage(_wpc));

        WavpackCloseFile(_wpc);
        _wpc = NULL;

        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }

        return NO;
    }

    if (!WavpackPackInit(_wpc)) {
        os_log_error(gSFBAudioEncoderLog, "WavpackPackInit failed: %{public}s", WavpackGetErrorMessage(_wpc));

        WavpackCloseFile(_wpc);
        _wpc = NULL;

        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }

        return NO;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
    CC_MD5_Init(&_md5);
#pragma clang diagnostic pop

    AudioStreamBasicDescription outputStreamDescription = {0};
    outputStreamDescription.mFormatID = kSFBAudioFormatWavPack;
    outputStreamDescription.mBitsPerChannel = _processingFormat.streamDescription->mBitsPerChannel;
    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription
                                                       channelLayout:_processingFormat.channelLayout];

    _framePosition = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_wpc) {
        WavpackCloseFile(_wpc);
        _wpc = NULL;
    }

    _firstBlock = nil;

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _wpc != NULL;
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

    if (!WavpackPackSamples(_wpc, (int32_t *)buffer.audioBufferList->mBuffers[0].mData, frameLength)) {
        os_log_error(gSFBAudioEncoderLog, "WavpackPackSamples failed: %{public}s", WavpackGetErrorMessage(_wpc));
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    // Update the MD5
    const int32_t *buf = buffer.audioBufferList->mBuffers[0].mData;
    switch (_config.bytes_per_sample) {
    case 1:
        for (AVAudioFrameCount i = 0; i < frameLength; ++i) {
            for (AVAudioFrameCount j = 0; j < _processingFormat.channelCount; ++j) {
                int8_t i8 = (int8_t)*buf;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
                CC_MD5_Update(&_md5, &i8, 1);
#pragma clang diagnostic pop
                ++buf;
            }
        }
        break;
    case 2:
        for (AVAudioFrameCount i = 0; i < frameLength; ++i) {
            for (AVAudioFrameCount j = 0; j < _processingFormat.channelCount; ++j) {
                int16_t i16 = (int16_t)*buf;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
                CC_MD5_Update(&_md5, &i16, 2);
#pragma clang diagnostic pop
                ++buf;
            }
        }
        break;
    case 3:
        for (AVAudioFrameCount i = 0; i < frameLength; ++i) {
            for (AVAudioFrameCount j = 0; j < _processingFormat.channelCount; ++j) {
                unsigned char hi = (unsigned char)((*buf >> 16) & 0xff);
                unsigned char mid = (unsigned char)((*buf >> 8) & 0xff);
                unsigned char lo = (unsigned char)(*buf & 0xff);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
                CC_MD5_Update(&_md5, &lo, 1);
                CC_MD5_Update(&_md5, &mid, 1);
                CC_MD5_Update(&_md5, &hi, 1);
#pragma clang diagnostic pop
                ++buf;
            }
        }
        break;
    case 4:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
        CC_MD5_Update(&_md5, buf, frameLength * 4);
#pragma clang diagnostic pop
        break;
    }

    _framePosition += frameLength;

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    if (!WavpackFlushSamples(_wpc)) {
        os_log_error(gSFBAudioEncoderLog, "WavpackFlushSamples failed: %{public}s", WavpackGetErrorMessage(_wpc));
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    unsigned char md5[CC_MD5_DIGEST_LENGTH];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
    CC_MD5_Final(md5, &_md5);
#pragma clang diagnostic pop
    if (!WavpackStoreMD5Sum(_wpc, md5)) {
        os_log_error(gSFBAudioEncoderLog, "WavpackStoreMD5Sum failed: %{public}s", WavpackGetErrorMessage(_wpc));
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    if (!WavpackFlushSamples(_wpc)) {
        os_log_error(gSFBAudioEncoderLog, "WavpackFlushSamples failed: %{public}s", WavpackGetErrorMessage(_wpc));
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    if (_estimatedFramesToEncode != _framePosition && _firstBlock) {
        WavpackUpdateNumSamples(_wpc, _firstBlock.mutableBytes);
        if (![_outputTarget seekToOffset:0 error:error]) {
            return NO;
        }
        if (![_outputTarget writeData:_firstBlock error:error]) {
            return NO;
        }
    }

    return YES;
}

@end
