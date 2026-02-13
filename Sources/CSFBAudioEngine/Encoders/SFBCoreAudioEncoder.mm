//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBCoreAudioEncoder.h"

#import "AudioFileWrapper.hpp"
#import "ExtAudioFileWrapper.hpp"
#import "SFBCStringForOSType.h"
#import "SFBLocalizedNameForURL.h"

#import <AudioToolbox/AudioToolbox.h>

#import <os/log.h>

#import <algorithm>
#import <vector>

SFBAudioEncoderName const SFBAudioEncoderNameCoreAudio = @"org.sbooth.AudioEngine.Encoder.CoreAudio";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFileTypeID = @"File Type ID";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatID = @"Format ID";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatFlags = @"Format Flags";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel = @"Bits per Channel";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings =
        @"Audio Converter Property Settings";

namespace {

template <typename T>
OSStatus setAudioConverterProperty(AudioConverterRef audioConverter, AudioConverterPropertyID propertyID,
                                   T propertyValue) noexcept {
    NSCParameterAssert(audioConverter != nullptr);
    return AudioConverterSetProperty(audioConverter, propertyID, sizeof(propertyValue), &propertyValue);
}

std::vector<AudioFileTypeID> typeIDsForExtension(NSString *pathExtension) noexcept {
    NSCParameterAssert(pathExtension != nil);
    CFStringRef extension = (__bridge CFStringRef)pathExtension;

    UInt32 size = 0;
    auto result =
            AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_TypesForExtension, sizeof(extension), &extension, &size);
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog,
                     "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_TypesForExtension) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return {};
    }

    auto typesForExtensionCount = size / sizeof(AudioFileTypeID);
    std::vector<AudioFileTypeID> typesForExtension(typesForExtensionCount);

    result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_TypesForExtension, sizeof(extension), &extension, &size,
                                    typesForExtension.data());
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog,
                     "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_TypesForExtension) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return {};
    }

    return typesForExtension;
}

std::vector<AudioFormatID> formatIDsForFileTypeID(AudioFileTypeID fileTypeID, bool forEncoding = false) noexcept {
    UInt32 size = 0;
    auto result =
            AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_AvailableFormatIDs, sizeof(fileTypeID), &fileTypeID, &size);
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog,
                     "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_AvailableFormatIDs) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return {};
    }

    auto availableFormatIDCount = size / sizeof(AudioFormatID);
    std::vector<AudioFormatID> availableFormatIDs(availableFormatIDCount);

    result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AvailableFormatIDs, sizeof(fileTypeID), &fileTypeID, &size,
                                    availableFormatIDs.data());
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog,
                     "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AvailableFormatIDs) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return {};
    }

    if (!forEncoding) {
        return availableFormatIDs;
    }

    result = AudioFormatGetPropertyInfo(kAudioFormatProperty_EncodeFormatIDs, 0, nullptr, &size);
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog,
                     "AudioFormatGetPropertyInfo (kAudioFormatProperty_EncodeFormatIDs) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return {};
    }

    auto encodeFormatIDCount = size / sizeof(AudioFormatID);
    std::vector<AudioFormatID> encodeFormatIDs(encodeFormatIDCount);

    result = AudioFormatGetProperty(kAudioFormatProperty_EncodeFormatIDs, 0, nullptr, &size, encodeFormatIDs.data());
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog,
                     "AudioFormatGetPropertyInfo (kAudioFormatProperty_EncodeFormatIDs) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        return {};
    }

    std::vector<AudioFormatID> formatIDs;
    std::set_intersection(encodeFormatIDs.begin(), encodeFormatIDs.end(), availableFormatIDs.begin(),
                          availableFormatIDs.end(), std::back_inserter(formatIDs));

    return formatIDs;
}

OSStatus readProc(void *inClientData, SInt64 inPosition, UInt32 requestCount, void *buffer,
                  UInt32 *actualCount) noexcept {
    NSCParameterAssert(inClientData != nullptr);

    SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
    SFBOutputTarget *outputTarget = encoder->_outputTarget;

    NSInteger offset;
    if (![outputTarget getOffset:&offset error:nil]) {
        return kAudioFileUnspecifiedError;
    }

    if (inPosition != offset) {
        if (!outputTarget.supportsSeeking) {
            return kAudioFileOperationNotSupportedError;
        }
        if (![outputTarget seekToOffset:inPosition error:nil]) {
            return kAudioFileUnspecifiedError;
        }
    }

    NSInteger bytesRead;
    if (![outputTarget readBytes:buffer length:(NSInteger)requestCount bytesRead:&bytesRead error:nil]) {
        return kAudioFileUnspecifiedError;
    }

    *actualCount = static_cast<UInt32>(bytesRead);

    return noErr;
}

OSStatus writeProc(void *inClientData, SInt64 inPosition, UInt32 requestCount, const void *buffer,
                   UInt32 *actualCount) noexcept {
    NSCParameterAssert(inClientData != nullptr);

    SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
    SFBOutputTarget *outputTarget = encoder->_outputTarget;

    NSInteger offset;
    if (![outputTarget getOffset:&offset error:nil]) {
        return kAudioFileUnspecifiedError;
    }

    if (inPosition != offset) {
        if (!outputTarget.supportsSeeking) {
            return kAudioFileOperationNotSupportedError;
        }
        if (![outputTarget seekToOffset:inPosition error:nil]) {
            return kAudioFileUnspecifiedError;
        }
    }

    NSInteger bytesWritten;
    if (![outputTarget writeBytes:buffer length:(NSInteger)requestCount bytesWritten:&bytesWritten error:nil]) {
        return kAudioFileUnspecifiedError;
    }

    *actualCount = static_cast<UInt32>(bytesWritten);

    return noErr;
}

SInt64 getSizeProc(void *inClientData) noexcept {
    NSCParameterAssert(inClientData != nullptr);

    SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
    SFBOutputTarget *outputTarget = encoder->_outputTarget;

    NSInteger length;
    if (![outputTarget getLength:&length error:nil]) {
        return -1;
    }

    return length;
}

OSStatus setSizeProc(void *inClientData, SInt64 inSize) noexcept {
    NSCParameterAssert(inClientData != nullptr);

    SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
    SFBOutputTarget *outputTarget = encoder->_outputTarget;

    // FIXME: Actually do something here
    (void)outputTarget;
    (void)inSize;

    return kAudioFileOperationNotSupportedError;
}

} /* namespace */

@interface SFBCoreAudioEncoder () {
  @private
    audio_toolbox::AudioFileWrapper _af;
    audio_toolbox::ExtAudioFileWrapper _eaf;
}
@end

@implementation SFBCoreAudioEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class] priority:-75];
}

+ (NSSet *)supportedPathExtensions {
    static NSSet *pathExtensions = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        UInt32 size = 0;
        auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size);
        if (result != noErr) {
            os_log_error(gSFBAudioEncoderLog,
                         "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            pathExtensions = [NSSet set];
            return;
        }

        auto writableTypesCount = size / sizeof(UInt32);
        std::vector<UInt32> writableTypes(writableTypesCount);

        result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size, writableTypes.data());
        if (result != noErr) {
            os_log_error(gSFBAudioEncoderLog,
                         "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            pathExtensions = [NSSet set];
            return;
        }

        NSMutableSet *supportedPathExtensions = [NSMutableSet set];
        for (UInt32 type : writableTypes) {
            CFArrayRef extensionsForType = nil;
            size = sizeof(extensionsForType);
            result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ExtensionsForType, sizeof(type), &type, &size,
                                            &extensionsForType);

            if (result == noErr) {
                [supportedPathExtensions addObjectsFromArray:(__bridge_transfer NSArray *)extensionsForType];
            } else {
                os_log_error(
                        gSFBAudioEncoderLog,
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
        auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size);
        if (result != noErr) {
            os_log_error(gSFBAudioEncoderLog,
                         "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            mimeTypes = [NSSet set];
            return;
        }

        auto writableTypesCount = size / sizeof(UInt32);
        std::vector<UInt32> writableTypes(writableTypesCount);

        result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size, writableTypes.data());
        if (result != noErr) {
            os_log_error(gSFBAudioEncoderLog,
                         "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            mimeTypes = [NSSet set];
            return;
        }

        NSMutableSet *supportedMIMETypes = [NSMutableSet set];
        for (UInt32 type : writableTypes) {
            CFArrayRef mimeTypesForType = nil;
            size = sizeof(mimeTypesForType);
            result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_MIMETypesForType, sizeof(type), &type, &size,
                                            &mimeTypesForType);

            if (result == noErr) {
                [supportedMIMETypes addObjectsFromArray:(__bridge_transfer NSArray *)mimeTypesForType];
            } else {
                os_log_error(gSFBAudioEncoderLog,
                             "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_MIMETypesForType) failed: %d '%{public}.4s'",
                             result, SFBCStringForOSType(result));
            }
        }

        mimeTypes = [supportedMIMETypes copy];
    });

    return mimeTypes;
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameCoreAudio;
}

- (BOOL)encodingIsLossless {
    switch (_outputFormat.streamDescription->mFormatID) {
    case kAudioFormatLinearPCM:
    case kAudioFormatAppleLossless:
    case kAudioFormatFLAC:
        return YES;
    default:
        // Be conservative and return NO for formats that aren't known to be lossless
        return NO;
    }
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat {
    NSParameterAssert(sourceFormat != nil);

    // Validate format
    if (sourceFormat.streamDescription->mFormatID != kAudioFormatLinearPCM) {
        return nil;
    }

    return sourceFormat;
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    AudioFileTypeID fileType = 0;
    if (NSNumber *fileTypeSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioFileTypeID];
        fileTypeSetting != nil) {
        fileType = static_cast<AudioFileTypeID>(fileTypeSetting.unsignedIntValue);
    } else {
        auto typesForExtension = typeIDsForExtension(_outputTarget.url.pathExtension);
        if (typesForExtension.empty()) {
            os_log_error(gSFBAudioEncoderLog,
                         "SFBAudioEncodingSettingsKeyCoreAudioFileTypeID is not set and extension \"%{public}@\" has "
                         "no known AudioFileTypeID",
                         _outputTarget.url.pathExtension);
            if (error != nullptr) {
                NSMutableDictionary *userInfo = [NSMutableDictionary
                        dictionaryWithObject:NSLocalizedString(
                                                     @"The file's extension does not match any known file type.", @"")
                                      forKey:NSLocalizedRecoverySuggestionErrorKey];

                if (_outputTarget.url != nil) {
                    userInfo[NSLocalizedDescriptionKey] = [NSString
                            localizedStringWithFormat:NSLocalizedString(
                                                              @"The type of the file “%@” could not be determined.",
                                                              @""),
                                                      SFBLocalizedNameForURL(_outputTarget.url)];
                    userInfo[NSURLErrorKey] = _outputTarget.url;
                } else {
                    userInfo[NSLocalizedDescriptionKey] =
                            NSLocalizedString(@"The type of the file could not be determined.", @"");
                }

                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInvalidFormat
                                         userInfo:userInfo];
            }
            return NO;
        }

        // There is no way to determine caller intent and select the most appropriate type; just use the first one
        fileType = typesForExtension[0];
        os_log_info(gSFBAudioEncoderLog,
                    "SFBAudioEncodingSettingsKeyCoreAudioFileTypeID is not set: guessed '%{public}.4s' based on "
                    "extension \"%{public}@\"",
                    SFBCStringForOSType(fileType), _outputTarget.url.pathExtension);
    }

    AudioFormatID formatID = 0;
    if (NSNumber *formatIDSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioFormatID];
        formatIDSetting != nil) {
        formatID = static_cast<AudioFormatID>(formatIDSetting.unsignedIntValue);
    } else {
        auto availableFormatIDs = formatIDsForFileTypeID(fileType, true);
        if (availableFormatIDs.empty()) {
            os_log_error(gSFBAudioEncoderLog,
                         "SFBAudioEncodingSettingsKeyCoreAudioFormatID is not set and file type '%{public}.4s' has no "
                         "known AudioFormatID",
                         SFBCStringForOSType(fileType));
            if (error != nullptr) {
                NSMutableDictionary *userInfo = [NSMutableDictionary
                        dictionaryWithObject:
                                NSLocalizedString(
                                        @"There are no supported audio formats for encoding files of this type.", @"")
                                      forKey:NSLocalizedRecoverySuggestionErrorKey];

                if (_outputTarget.url != nil) {
                    userInfo[NSLocalizedDescriptionKey] = [NSString
                            localizedStringWithFormat:NSLocalizedString(
                                                              @"The file “%@” is an unsupported audio format.", @""),
                                                      SFBLocalizedNameForURL(_outputTarget.url)];
                    userInfo[NSURLErrorKey] = _outputTarget.url;
                } else {
                    userInfo[NSLocalizedDescriptionKey] =
                            NSLocalizedString(@"The file is an unsupported audio format.", @"");
                }

                *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                             code:SFBAudioEncoderErrorCodeInvalidFormat
                                         userInfo:userInfo];
            }
            return NO;
        }

        // There is no way to determine caller intent and select the most appropriate format; use PCM if available,
        // otherwise use the first one
        formatID = availableFormatIDs[0];
        auto result = std::find(std::cbegin(availableFormatIDs), std::cend(availableFormatIDs), kAudioFormatLinearPCM);
        if (result != std::cend(availableFormatIDs)) {
            formatID = *result;
        }
        os_log_info(gSFBAudioEncoderLog,
                    "SFBAudioEncodingSettingsKeyCoreAudioFormatID is not set: guessed '%{public}.4s' based on format "
                    "'%{public}.4s'",
                    SFBCStringForOSType(formatID), SFBCStringForOSType(fileType));
    }

    UInt32 formatFlags = 0;
    if (NSNumber *formatFlagsSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioFormatFlags];
        formatFlagsSetting != nil) {
        formatFlags = static_cast<UInt32>(formatFlagsSetting.unsignedIntValue);
    } else {
        os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioFormatFlags is not set; mFormatFlags "
                                         "will be zero which is probably incorrect");
    }

    UInt32 bitsPerChannel = 0;
    if (NSNumber *bitsPerChannelSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel];
        bitsPerChannelSetting != nil) {
        bitsPerChannel = static_cast<UInt32>(bitsPerChannelSetting.unsignedIntValue);
    } else {
        os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel is not set; "
                                         "mBitsPerChannel will be zero which is probably incorrect");
    }

    AudioStreamBasicDescription format{};

    format.mFormatID = formatID;
    format.mFormatFlags = formatFlags;
    format.mBitsPerChannel = bitsPerChannel;
    format.mSampleRate = _processingFormat.sampleRate;
    format.mChannelsPerFrame = _processingFormat.channelCount;

    // Flesh out output structure for PCM formats
    if (format.mFormatID == kAudioFormatLinearPCM) {
        format.mBytesPerPacket = format.mChannelsPerFrame * ((format.mBitsPerChannel + 7) / 8);
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = format.mBytesPerPacket / format.mFramesPerPacket;
    }
    // Adjust the flags for Apple Lossless and FLAC
    else if (format.mFormatID == kAudioFormatAppleLossless || format.mFormatID == kAudioFormatFLAC) {
        switch (_processingFormat.streamDescription->mBitsPerChannel) {
        case 16:
            format.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
            break;
        case 20:
            format.mFormatFlags = kAppleLosslessFormatFlag_20BitSourceData;
            break;
        case 24:
            format.mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;
            break;
        case 32:
            format.mFormatFlags = kAppleLosslessFormatFlag_32BitSourceData;
            break;
        default:
            format.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
            break;
        }
    }
    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format
                                                       channelLayout:_processingFormat.channelLayout];

    AudioFileID audioFile;
    auto result = AudioFileInitializeWithCallbacks((__bridge void *)self, readProc, writeProc, getSizeProc, setSizeProc,
                                                   fileType, &format, 0, &audioFile);
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog, "AudioFileOpenWithCallbacks failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
        }
        return NO;
    }

    auto af = audio_toolbox::AudioFileWrapper(audioFile);

    ExtAudioFileRef extAudioFile;
    result = ExtAudioFileWrapAudioFileID(af, true, &extAudioFile);
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog, "ExtAudioFileWrapAudioFileID failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
        }
        return NO;
    }

    auto eaf = audio_toolbox::ExtAudioFileWrapper(extAudioFile);

    result = ExtAudioFileSetProperty(eaf, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription),
                                     _processingFormat.streamDescription);
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog,
                     "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %d '%{public}.4s'",
                     result, SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
        }
        return NO;
    }

    if (_processingFormat.channelLayout) {
        result = ExtAudioFileSetProperty(eaf, kExtAudioFileProperty_ClientChannelLayout,
                                         sizeof(_processingFormat.channelLayout.layout),
                                         _processingFormat.channelLayout.layout);
        if (result != noErr) {
            os_log_error(
                    gSFBAudioEncoderLog,
                    "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientChannelLayout) failed: %d '%{public}.4s'",
                    result, SFBCStringForOSType(result));
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
            }
            return NO;
        }
    }

    if (NSDictionary *audioConverterPropertySettings =
                [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings];
        audioConverterPropertySettings != nil) {
        AudioConverterRef audioConverter = nullptr;
        UInt32 size = sizeof(audioConverter);
        result = ExtAudioFileGetProperty(extAudioFile, kExtAudioFileProperty_AudioConverter, &size, &audioConverter);
        if (result != noErr) {
            os_log_error(gSFBAudioEncoderLog,
                         "ExtAudioFileGetProperty (kExtAudioFileProperty_AudioConverter) failed: %d '%{public}.4s'",
                         result, SFBCStringForOSType(result));
            if (error != nullptr) {
                *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
            }
            return NO;
        }

        if (audioConverter != nullptr) {
            for (NSNumber *key in audioConverterPropertySettings) {
                AudioConverterPropertyID propertyID = static_cast<AudioConverterPropertyID>(key.unsignedIntValue);
                switch (propertyID) {
                case kAudioConverterSampleRateConverterComplexity:
                    result = setAudioConverterProperty<OSType>(
                            audioConverter, propertyID,
                            [[audioConverterPropertySettings objectForKey:key] unsignedIntValue]);
                    break;
                case kAudioConverterSampleRateConverterQuality:
                case kAudioConverterCodecQuality:
                case kAudioConverterEncodeBitRate:
                case kAudioCodecPropertyBitRateControlMode:
                case kAudioCodecPropertySoundQualityForVBR:
                case kAudioCodecPropertyBitRateForVBR:
#if !TARGET_OS_IPHONE
                case kAudioConverterPropertyDithering:
                case kAudioConverterPropertyDitherBitDepth:
#endif
                    result = setAudioConverterProperty<UInt32>(
                            audioConverter, propertyID,
                            [[audioConverterPropertySettings objectForKey:key] unsignedIntValue]);
                    break;
                default:
                    os_log_info(gSFBAudioEncoderLog, "Ignoring unknown AudioConverterPropertyID: %d '%{public}.4s'",
                                propertyID, SFBCStringForOSType(propertyID));
                    break;
                }

                if (result != noErr) {
                    os_log_error(gSFBAudioEncoderLog,
                                 "AudioConverterSetProperty ('%{public}.4s') failed: %d '%{public}.4s'",
                                 SFBCStringForOSType(propertyID), result, SFBCStringForOSType(result));
                    if (error != nullptr) {
                        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
                    }
                    return NO;
                }
            }

            // Notify ExtAudioFile about the converter property changes
            CFArrayRef converterConfig = nullptr;
            result = ExtAudioFileSetProperty(eaf, kExtAudioFileProperty_ConverterConfig, sizeof(converterConfig),
                                             &converterConfig);
            if (result != noErr) {
                os_log_error(
                        gSFBAudioEncoderLog,
                        "ExtAudioFileSetProperty (kExtAudioFileProperty_ConverterConfig) failed: %d '%{public}.4s'",
                        result, SFBCStringForOSType(result));
                if (error != nullptr) {
                    *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
                }
                return NO;
            }
        } else {
            os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings is "
                                             "set but kExtAudioFileProperty_AudioConverter is null");
        }
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
    OSStatus status = ExtAudioFileTell(_eaf, &currentFrame);
    if (status != noErr) {
        os_log_error(gSFBAudioEncoderLog, "ExtAudioFileTell failed: %d '%{public}.4s'", status,
                     SFBCStringForOSType(status));
        return SFBUnknownFramePosition;
    }
    return currentFrame;
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    frameLength = std::min(frameLength, buffer.frameLength);
    if (frameLength == 0) {
        return YES;
    }

    auto result = ExtAudioFileWrite(_eaf, frameLength, buffer.audioBufferList);
    if (result != noErr) {
        os_log_error(gSFBAudioEncoderLog, "ExtAudioFileWrite failed: %d '%{public}.4s'", result,
                     SFBCStringForOSType(result));
        if (error != nullptr) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
        }
        return NO;
    }

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    return YES;
}

@end
