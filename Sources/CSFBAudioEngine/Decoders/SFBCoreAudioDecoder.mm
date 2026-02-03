//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBCoreAudioDecoder.h"

#import "AudioFileWrapper.hpp"
#import "ExtAudioFileWrapper.hpp"
#import "NSData+SFBExtensions.h"
#import "SFBCStringForOSType.h"
#import "SFBLocalizedNameForURL.h"

#import <AudioToolbox/AudioToolbox.h>

#import <os/log.h>

#import <algorithm>
#import <cstdlib>
#import <vector>

SFBAudioDecoderName const SFBAudioDecoderNameCoreAudio = @"org.sbooth.AudioEngine.Decoder.CoreAudio";

namespace {

// ========================================
// Callbacks
OSStatus readCallback(void *inClientData, SInt64 inPosition, UInt32 requestCount, void *buffer,
                      UInt32 *actualCount) noexcept {
    NSCParameterAssert(inClientData != nullptr);

    SFBCoreAudioDecoder *decoder = (__bridge SFBCoreAudioDecoder *)inClientData;

    NSInteger offset;
    if (![decoder->_inputSource getOffset:&offset error:nil]) {
        return kAudioFileUnspecifiedError;
    }

    if (inPosition != offset) {
        if (!decoder->_inputSource.supportsSeeking) {
            return kAudioFileOperationNotSupportedError;
        }
        if (![decoder->_inputSource seekToOffset:inPosition error:nil]) {
            return kAudioFileUnspecifiedError;
        }
    }

    NSInteger bytesRead;
    if (![decoder->_inputSource readBytes:buffer length:requestCount bytesRead:&bytesRead error:nil]) {
        return kAudioFileUnspecifiedError;
    }

    *actualCount = static_cast<UInt32>(bytesRead);

    if (decoder->_inputSource.atEOF) {
        return kAudioFileEndOfFileError;
    }

    return noErr;
}

SInt64 getSizeCallback(void *inClientData) noexcept {
    NSCParameterAssert(inClientData != nullptr);

    SFBCoreAudioDecoder *decoder = (__bridge SFBCoreAudioDecoder *)inClientData;

    NSInteger length;
    if (![decoder->_inputSource getLength:&length error:nil]) {
        return -1;
    }
    return length;
}

NSError *formatNotRecognizedError(NSURL *_Nullable url, OSStatus result) noexcept {
    NSMutableDictionary *userInfo = [NSMutableDictionary
            dictionaryWithObjectsAndKeys:NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                                         NSLocalizedRecoverySuggestionErrorKey,
                                         [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil],
                                         NSUnderlyingErrorKey, nil];

    if (url != nil) {
        userInfo[NSLocalizedDescriptionKey] = [NSString
                localizedStringWithFormat:NSLocalizedString(@"The format of the file “%@” was not recognized.", @""),
                                          SFBLocalizedNameForURL(url)];
        userInfo[NSURLErrorKey] = url;
    } else {
        userInfo[NSLocalizedDescriptionKey] = NSLocalizedString(@"The format of the file was not recognized.", @"");
    }

    return [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                               code:SFBAudioDecoderErrorCodeInvalidFormat
                           userInfo:userInfo];
}

} /* namespace */

@interface SFBCoreAudioDecoder () {
  @private
    audio_toolbox::AudioFileWrapper _af;
    audio_toolbox::ExtAudioFileWrapper _eaf;
}
@end

@implementation SFBCoreAudioDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class] priority:-75];
}

+ (NSSet *)supportedPathExtensions {
    static NSSet *pathExtensions = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        UInt32 size = 0;
        auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size);
        if (result != noErr) {
            os_log_error(gSFBAudioDecoderLog,
                         "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            pathExtensions = [NSSet set];
            return;
        }

        auto readableTypesCount = size / sizeof(UInt32);
        std::vector<UInt32> readableTypes(readableTypesCount);

        result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size, readableTypes.data());
        if (result != noErr) {
            os_log_error(gSFBAudioDecoderLog,
                         "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            pathExtensions = [NSSet set];
            return;
        }

        NSMutableSet *supportedPathExtensions = [NSMutableSet set];
        for (UInt32 type : readableTypes) {
            CFArrayRef extensionsForType = nil;
            size = sizeof(extensionsForType);
            result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ExtensionsForType, sizeof(type), &type, &size,
                                            &extensionsForType);

            if (result == noErr) {
                [supportedPathExtensions addObjectsFromArray:(__bridge_transfer NSArray *)extensionsForType];
            } else {
                os_log_error(
                        gSFBAudioDecoderLog,
                        "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_ExtensionsForType) failed: %d '%{public}.4s'",
                        result, SFBCStringForOSType(result));
            }
        }

        pathExtensions = [supportedPathExtensions copy];
    });

    return pathExtensions;
}

+ (NSSet *)supportedMIMETypes {
    static NSSet *mimeTypes = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        UInt32 size = 0;
        auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size);
        if (result != noErr) {
            os_log_error(gSFBAudioDecoderLog,
                         "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            mimeTypes = [NSSet set];
            return;
        }

        auto readableTypesCount = size / sizeof(UInt32);
        std::vector<UInt32> readableTypes(readableTypesCount);

        result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size, readableTypes.data());
        if (result != noErr) {
            os_log_error(gSFBAudioDecoderLog,
                         "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            mimeTypes = [NSSet set];
            return;
        }

        NSMutableSet *supportedMIMETypes = [NSMutableSet set];
        for (UInt32 type : readableTypes) {
            CFArrayRef mimeTypesForType = nil;
            size = sizeof(mimeTypesForType);
            result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_MIMETypesForType, sizeof(type), &type, &size,
                                            &mimeTypesForType);

            if (result == noErr) {
                [supportedMIMETypes addObjectsFromArray:(__bridge_transfer NSArray *)mimeTypesForType];
            } else {
                os_log_error(gSFBAudioDecoderLog,
                             "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_MIMETypesForType) failed: %d '%{public}.4s'",
                             result, SFBCStringForOSType(result));
            }
        }

        mimeTypes = [supportedMIMETypes copy];
    });

    return mimeTypes;
}

+ (SFBAudioDecoderName)decoderName {
    return SFBAudioDecoderNameCoreAudio;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [inputSource readHeaderOfLength:std::max({SFBMPEG4DetectionSize, SFBCAFDetectionSize,
                                                               SFBAIFFDetectionSize, SFBWAVEDetectionSize})
                                        skipID3v2Tag:NO
                                               error:error];
    if (header == nil) {
        return NO;
    }

    *formatIsSupported = SFBTernaryTruthValueUnknown;

    // Core Audio supports a multitude of formats. This is not meant to be an exhaustive check but
    // just something quick to identify common file formats lacking a path extension or MIME type.

    if ([header isMPEG4Header]) {
        // M4A files
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else if ([header isCAFHeader]) {
        // CAF files
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else if ([header isAIFFHeader]) {
        // AIFF and AIFF-C files
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else if ([header isWAVEHeader]) {
        // WAVE files
        *formatIsSupported = SFBTernaryTruthValueTrue;
    }

    return YES;
}

- (BOOL)decodingIsLossless {
    switch (_sourceFormat.streamDescription->mFormatID) {
    case kAudioFormatLinearPCM:
    case kAudioFormatAppleLossless:
    case kAudioFormatFLAC:
        return YES;
    default:
        // Be conservative and return NO for formats that aren't known to be lossless
        return NO;
    }
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    // Open the input file
    AudioFileID audioFile;
    auto result = AudioFileOpenWithCallbacks((__bridge void *)self, readCallback, nullptr, getSizeCallback, nullptr, 0,
                                             &audioFile);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog, "AudioFileOpenWithCallbacks failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = formatNotRecognizedError(_inputSource.url, result);
        }
        return NO;
    }

    auto af = audio_toolbox::AudioFileWrapper(audioFile);

    ExtAudioFileRef extAudioFile;
    result = ExtAudioFileWrapAudioFileID(af, false, &extAudioFile);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog, "ExtAudioFileWrapAudioFileID failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = formatNotRecognizedError(_inputSource.url, result);
        }
        return NO;
    }

    auto eaf = audio_toolbox::ExtAudioFileWrapper(extAudioFile);

    // Query file format
    AudioStreamBasicDescription format{};
    UInt32 dataSize = sizeof(format);
    result = ExtAudioFileGetProperty(eaf, kExtAudioFileProperty_FileDataFormat, &dataSize, &format);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog,
                     "ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        if (error != nullptr) {
            NSDictionary *userInfo = nil;
            if (_inputSource.url != nil) {
                userInfo = [NSDictionary dictionaryWithObject:_inputSource.url forKey:NSURLErrorKey];
            }
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:userInfo];
        }
        return NO;
    }

    // Query channel layout
    AVAudioChannelLayout *channelLayout = nil;
    result = ExtAudioFileGetPropertyInfo(eaf, kExtAudioFileProperty_FileChannelLayout, &dataSize, nullptr);
    if (result == noErr) {
        AudioChannelLayout *layout = static_cast<AudioChannelLayout *>(std::malloc(dataSize));
        result = ExtAudioFileGetProperty(eaf, kExtAudioFileProperty_FileChannelLayout, &dataSize, layout);
        if (result != noErr) {
            os_log_error(gSFBAudioDecoderLog,
                         "ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));

            std::free(layout);

            if (error != nullptr) {
                *error = formatNotRecognizedError(_inputSource.url, result);
            }
            return NO;
        }

        channelLayout = [[AVAudioChannelLayout alloc] initWithLayout:layout];
        std::free(layout);

        // ExtAudioFile occasionally returns empty channel layouts; ignore them
        if (channelLayout.channelCount != format.mChannelsPerFrame) {
            os_log_error(
                    gSFBAudioDecoderLog,
                    "Channel count mismatch between AudioStreamBasicDescription (%u) and AVAudioChannelLayout (%u)",
                    format.mChannelsPerFrame, channelLayout.channelCount);
            channelLayout = nil;
        }
    } else {
        os_log_error(gSFBAudioDecoderLog,
                     "ExtAudioFileGetPropertyInfo (kExtAudioFileProperty_FileChannelLayout) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
    }

    _sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format channelLayout:channelLayout];

    // Tell the ExtAudioFile the format in which we'd like our data

    if (format.mFormatID == kAudioFormatLinearPCM) {
        // For Linear PCM formats leave the data untouched
        _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format channelLayout:channelLayout];
    } else if (format.mFormatID == kAudioFormatAppleLossless || format.mFormatID == kAudioFormatFLAC) {
        // For Apple Lossless and FLAC convert to packed ints if possible, otherwise high-align
        AudioStreamBasicDescription asbd{};

        asbd.mFormatID = kAudioFormatLinearPCM;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
        asbd.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;
#pragma clang diagnostic pop

        asbd.mSampleRate = format.mSampleRate;
        asbd.mChannelsPerFrame = format.mChannelsPerFrame;

        if (format.mFormatFlags == kAppleLosslessFormatFlag_16BitSourceData) {
            asbd.mBitsPerChannel = 16;
        } else if (format.mFormatFlags == kAppleLosslessFormatFlag_20BitSourceData) {
            asbd.mBitsPerChannel = 20;
        } else if (format.mFormatFlags == kAppleLosslessFormatFlag_24BitSourceData) {
            asbd.mBitsPerChannel = 24;
        } else if (format.mFormatFlags == kAppleLosslessFormatFlag_32BitSourceData) {
            asbd.mBitsPerChannel = 32;
        }

        asbd.mFormatFlags |= asbd.mBitsPerChannel % 8 ? kAudioFormatFlagIsAlignedHigh : kAudioFormatFlagIsPacked;

        asbd.mBytesPerPacket = ((asbd.mBitsPerChannel + 7) / 8) * asbd.mChannelsPerFrame;
        asbd.mFramesPerPacket = 1;
        asbd.mBytesPerFrame = asbd.mBytesPerPacket / asbd.mFramesPerPacket;

        _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&asbd channelLayout:channelLayout];
    } else {
        // For all other formats convert to the canonical Core Audio format
        if (channelLayout != nil) {
            _processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                                 sampleRate:format.mSampleRate
                                                                interleaved:NO
                                                              channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                                 sampleRate:format.mSampleRate
                                                                   channels:format.mChannelsPerFrame
                                                                interleaved:NO];
        }
    }

    // For audio with more than 2 channels AVAudioFormat requires a channel map. Since ExtAudioFile doesn't always
    // return one, there is a chance that the initialization of _processingFormat failed. If that happened then
    // attempting to set kExtAudioFileProperty_ClientDataFormat will segfault
    if (_processingFormat == nil) {
        if (error != nullptr) {
            *error = formatNotRecognizedError(_inputSource.url, result);
        }
        return NO;
    }

    result = ExtAudioFileSetProperty(eaf, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription),
                                     _processingFormat.streamDescription);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog,
                     "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = formatNotRecognizedError(_inputSource.url, result);
        }
        return NO;
    }

    _af = std::move(af);
    _eaf = std::move(eaf);

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _eaf.reset();
    _af.reset();

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _eaf != nullptr;
}

- (AVAudioFramePosition)framePosition {
    SInt64 currentFrame;
    auto result = ExtAudioFileTell(_eaf, &currentFrame);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog, "ExtAudioFileTell failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        return SFBUnknownFramePosition;
    }
    return currentFrame;
}

- (AVAudioFramePosition)frameLength {
    SInt64 frameLength;
    UInt32 dataSize = sizeof(frameLength);
    auto result = ExtAudioFileGetProperty(_eaf, kExtAudioFileProperty_FileLengthFrames, &dataSize, &frameLength);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog,
                     "ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return SFBUnknownFrameLength;
    }
    return frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    frameLength = std::min(frameLength, buffer.frameCapacity);
    if (frameLength == 0) {
        buffer.frameLength = 0;
        return YES;
    }

    buffer.frameLength = buffer.frameCapacity;

    auto result = ExtAudioFileRead(_eaf, &frameLength, buffer.mutableAudioBufferList);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog, "ExtAudioFileRead failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        buffer.frameLength = 0;
        if (error != nullptr) {
            NSError *decodingError = [self genericDecodingError];
            NSMutableDictionary *userInfo = [decodingError.userInfo mutableCopy];
            userInfo[NSUnderlyingErrorKey] = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
            *error = [NSError errorWithDomain:decodingError.domain code:decodingError.code userInfo:userInfo];
        }
        return NO;
    }

    buffer.frameLength = frameLength;

    return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    NSParameterAssert(frame >= 0);
    auto result = ExtAudioFileSeek(_eaf, frame);
    if (result != noErr) {
        os_log_error(gSFBAudioDecoderLog, "ExtAudioFileSeek failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        if (error != nullptr) {
            NSError *seekError = [self genericSeekError];
            NSMutableDictionary *userInfo = [seekError.userInfo mutableCopy];
            userInfo[NSUnderlyingErrorKey] = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
            *error = [NSError errorWithDomain:seekError.domain code:seekError.code userInfo:userInfo];
        }
        return NO;
    }
    return YES;
}

@end
