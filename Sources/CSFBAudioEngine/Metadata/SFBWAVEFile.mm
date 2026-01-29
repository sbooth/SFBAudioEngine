//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBWAVEFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/tfilestream.h>
#import <taglib/wavfile.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameWAVE = @"org.sbooth.AudioEngine.File.WAVE";

@implementation SFBWAVEFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithArray:@[ @"wav", @"wave" ]];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/wave"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameWAVE;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [fileHandle readHeaderOfLength:SFBWAVEDetectionSize skipID3v2Tag:NO error:error];
    if (header == nil) {
        return NO;
    }

    if ([header isWAVEHeader]) {
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else {
        *formatIsSupported = SFBTernaryTruthValueFalse;
    }

    return YES;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error {
    try {
        TagLib::FileStream stream(self.url.fileSystemRepresentation, true);
        if (!stream.isOpen()) {
            if (error != nullptr) {
                *error = [self genericOpenForReadingError];
            }
            return NO;
        }

        TagLib::RIFF::WAV::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"WAVE", @"")];
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"WAVE" forKey:SFBAudioPropertiesKeyFormatName];
        if (const auto *properties = file.audioProperties(); properties != nullptr) {
            sfb::addAudioPropertiesToDictionary(properties, propertiesDictionary);

            if (properties->bitsPerSample() != 0) {
                propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
            }
            if (properties->sampleFrames() != 0) {
                propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(properties->sampleFrames());
            }
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (file.hasInfoTag()) {
            [metadata addMetadataFromTagLibTag:file.InfoTag()];
        }

        if (file.hasID3v2Tag()) {
            [metadata addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];
        }

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error reading WAVE properties and metadata: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

- (BOOL)writeMetadataReturningError:(NSError **)error {
    try {
        TagLib::FileStream stream(self.url.fileSystemRepresentation);
        if (!stream.isOpen()) {
            if (error != nullptr) {
                *error = [self genericOpenForWritingError];
            }
            return NO;
        }

        TagLib::RIFF::WAV::File file(&stream, false);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"WAVE", @"")];
            }
            return NO;
        }

        // An Info tag is only written if present, but ID3v2 tags are always written

        // TODO: Should other field names from the Info tag be handled?
        if (file.hasInfoTag()) {
            sfb::setTagFromMetadata(self.metadata, file.InfoTag());
        }

        sfb::setID3v2TagFromMetadata(self.metadata, file.ID3v2Tag());

        if (!file.save()) {
            if (error != nullptr) {
                *error = [self genericSaveError];
            }
            return NO;
        }

        return YES;
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error writing WAVE metadata: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

@end
