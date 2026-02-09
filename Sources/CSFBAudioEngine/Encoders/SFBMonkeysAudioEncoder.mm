//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <AudioToolbox/AudioToolbox.h>

#import <os/log.h>

#import <algorithm>
#import <exception>
#import <memory>

#define PLATFORM_APPLE

#import <MAC/All.h>
#import <MAC/IAPEIO.h>
#import <MAC/MACLib.h>

#undef PLATFORM_APPLE

#import "SFBCStringForOSType.h"
#import "SFBMonkeysAudioEncoder.h"

SFBAudioEncoderName const SFBAudioEncoderNameMonkeysAudio = @"org.sbooth.AudioEngine.Encoder.MonkeysAudio";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyAPECompressionLevel = @"Compression Level";

SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelFast = @"Fast";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelNormal =
        @"Normal";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelHigh = @"High";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelExtraHigh =
        @"Extra High";
SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelInsane =
        @"Insane";

namespace {

// The I/O interface for MAC
class APEIOInterface final : public APE::IAPEIO {
  public:
    explicit APEIOInterface(SFBOutputTarget *outputTarget) : outputTarget_(outputTarget) {}

    int Open(const wchar_t *pName, bool bOpenReadOnly) override {
#pragma unused(pName)
#pragma unused(bOpenReadOnly)

        return ERROR_INVALID_INPUT_FILE;
    }

    int Close() override { return ERROR_SUCCESS; }

    int Read(void *pBuffer, unsigned int nBytesToRead, unsigned int *pBytesRead) override {
        NSInteger bytesRead;
        if (![outputTarget_ readBytes:pBuffer length:nBytesToRead bytesRead:&bytesRead error:nil]) {
            return ERROR_IO_READ;
        }

        *pBytesRead = static_cast<unsigned int>(bytesRead);

        return ERROR_SUCCESS;
    }

    int Write(const void *pBuffer, unsigned int nBytesToWrite, unsigned int *pBytesWritten) override {
        NSInteger bytesWritten;
        if (![outputTarget_ writeBytes:pBuffer length:(NSInteger)nBytesToWrite bytesWritten:&bytesWritten error:nil] ||
            bytesWritten != nBytesToWrite) {
            return ERROR_IO_WRITE;
        }

        *pBytesWritten = static_cast<unsigned int>(bytesWritten);

        return ERROR_SUCCESS;
    }

    int Seek(APE::int64 nPosition, APE::SeekMethod nMethod) override {
        if (!outputTarget_.supportsSeeking) {
            return ERROR_IO_READ;
        }

        NSInteger offset = nPosition;
        switch (nMethod) {
        case APE::SeekFileBegin:
            // offset remains unchanged
            break;
        case APE::SeekFileCurrent:
            if (NSInteger outputTargetOffset; [outputTarget_ getOffset:&outputTargetOffset error:nil]) {
                offset += outputTargetOffset;
            }
            break;
        case APE::SeekFileEnd:
            if (NSInteger outputTargetLength; [outputTarget_ getLength:&outputTargetLength error:nil]) {
                offset += outputTargetLength;
            }
            break;
        }

        if (![outputTarget_ seekToOffset:offset error:nil]) {
            return ERROR_IO_READ;
        }

        return ERROR_SUCCESS;
    }

    int Create(const wchar_t *pName) override {
#pragma unused(pName)
        return ERROR_IO_WRITE;
    }

    int Delete() override { return ERROR_IO_WRITE; }

    int SetEOF() override { return ERROR_IO_WRITE; }

    unsigned char *GetBuffer(int *pnBufferBytes) override {
#pragma unused(pnBufferBytes)
        return nullptr;
    }

    APE::int64 GetPosition() override {
        NSInteger offset;
        if (![outputTarget_ getOffset:&offset error:nil]) {
            return -1;
        }
        return offset;
    }

    APE::int64 GetSize() override {
        NSInteger length;
        if (![outputTarget_ getLength:&length error:nil]) {
            return -1;
        }
        return length;
    }

    int GetName(wchar_t *pBuffer) override {
#pragma unused(pBuffer)
        return ERROR_SUCCESS;
    }

  private:
    SFBOutputTarget *outputTarget_;
};

} /* namespace */

@interface SFBMonkeysAudioEncoder () {
  @private
    std::unique_ptr<APEIOInterface> _ioInterface;
    std::unique_ptr<APE::IAPECompress> _compressor;
    AVAudioFramePosition _framePosition;
}
@end

@implementation SFBMonkeysAudioEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"ape"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/monkeys-audio", @"audio/x-monkeys-audio" ]];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameMonkeysAudio;
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

    APE::WAVEFORMATEX wve;
    auto result = FillWaveFormatEx(&wve, WAVE_FORMAT_PCM, static_cast<int>(sourceFormat.sampleRate),
                                   static_cast<int>(sourceFormat.streamDescription->mBitsPerChannel),
                                   static_cast<int>(sourceFormat.channelCount));
    if (result != ERROR_SUCCESS) {
        os_log_error(gSFBAudioEncoderLog, "FillWaveFormatEx() failed: %d", result);
        return nil;
    }

    // Set up the processing format
    AudioStreamBasicDescription streamDescription{};

    streamDescription.mFormatID = kAudioFormatLinearPCM;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
    streamDescription.mFormatFlags =
            kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
#pragma clang diagnostic pop

    streamDescription.mSampleRate = wve.nSamplesPerSec;
    streamDescription.mChannelsPerFrame = wve.nChannels;
    streamDescription.mBitsPerChannel = wve.wBitsPerSample;

    streamDescription.mBytesPerPacket = ((wve.wBitsPerSample + 7) / 8) * streamDescription.mChannelsPerFrame;
    streamDescription.mFramesPerPacket = 1;
    streamDescription.mBytesPerFrame = streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

    // Use WAVFORMATEX channel order
    AVAudioChannelLayout *channelLayout = nil;

    if (sourceFormat.channelLayout != nil) {
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
        } else {
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

    try {
        int result;
        auto *compressor = CreateIAPECompress(&result);
        if (compressor == nullptr) {
            os_log_error(gSFBAudioEncoderLog, "CreateIAPECompress() failed: %d", result);
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }

        _compressor = std::unique_ptr<APE::IAPECompress>(compressor);
        _ioInterface = std::make_unique<APEIOInterface>(_outputTarget);
    } catch (const std::exception &e) {
        os_log_error(gSFBAudioEncoderLog, "Error creating Monkey's Audio encoder: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    int compressionLevel = APE_COMPRESSION_LEVEL_NORMAL;
    if (SFBAudioEncodingSettingsValue level = [_settings objectForKey:SFBAudioEncodingSettingsKeyAPECompressionLevel];
        level != nil) {
        if (level == SFBAudioEncodingSettingsValueAPECompressionLevelFast) {
            compressionLevel = APE_COMPRESSION_LEVEL_FAST;
        } else if (level == SFBAudioEncodingSettingsValueAPECompressionLevelNormal) {
            compressionLevel = APE_COMPRESSION_LEVEL_NORMAL;
        } else if (level == SFBAudioEncodingSettingsValueAPECompressionLevelHigh) {
            compressionLevel = APE_COMPRESSION_LEVEL_HIGH;
        } else if (level == SFBAudioEncodingSettingsValueAPECompressionLevelExtraHigh) {
            compressionLevel = APE_COMPRESSION_LEVEL_EXTRA_HIGH;
        } else if (level == SFBAudioEncodingSettingsValueAPECompressionLevelInsane) {
            compressionLevel = APE_COMPRESSION_LEVEL_INSANE;
        } else {
            os_log_info(gSFBAudioEncoderLog, "Ignoring unknown APE compression level: %{public}@", level);
        }
    }

    APE::WAVEFORMATEX wve;
    auto result = FillWaveFormatEx(&wve, WAVE_FORMAT_PCM, static_cast<int>(_sourceFormat.sampleRate),
                                   static_cast<int>(_sourceFormat.streamDescription->mBitsPerChannel),
                                   static_cast<int>(_sourceFormat.channelCount));
    if (result != ERROR_SUCCESS) {
        os_log_error(gSFBAudioEncoderLog, "FillWaveFormatEx() failed: %d", result);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInvalidFormat
                                     userInfo:nil];
        }
        return NO;
    }

    result = _compressor->StartEx(_ioInterface.get(), &wve, false, MAX_AUDIO_BYTES_UNKNOWN, compressionLevel);
    if (result != ERROR_SUCCESS) {
        os_log_error(gSFBAudioEncoderLog, "_compressor->StartEx() failed: %d", result);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInvalidFormat
                                     userInfo:nil];
        }
        return NO;
    }

    AudioStreamBasicDescription outputStreamDescription{};
    outputStreamDescription.mFormatID = kSFBAudioFormatMonkeysAudio;
    outputStreamDescription.mBitsPerChannel = wve.wBitsPerSample;
    outputStreamDescription.mSampleRate = wve.nSamplesPerSec;
    outputStreamDescription.mChannelsPerFrame = wve.nChannels;
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription
                                                       channelLayout:_processingFormat.channelLayout];

    _framePosition = 0;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _ioInterface.reset();
    _compressor.reset();

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _compressor != nullptr;
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

    auto bytesToWrite = frameLength * _processingFormat.streamDescription->mBytesPerFrame;
    auto result = _compressor->AddData((unsigned char *)buffer.audioBufferList->mBuffers[0].mData, bytesToWrite);
    if (result != ERROR_SUCCESS) {
        os_log_error(gSFBAudioEncoderLog, "_compressor->AddData() failed: %lld", result);
        if (error != nullptr) {
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
    auto result = _compressor->Finish(nullptr, 0, 0);
    if (result != ERROR_SUCCESS) {
        os_log_error(gSFBAudioEncoderLog, "_compressor->Finish() failed: %d", result);
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
