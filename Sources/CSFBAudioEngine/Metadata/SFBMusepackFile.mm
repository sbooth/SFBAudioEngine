//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBMusepackFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibAPETag.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/mpcfile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameMusepack = @"org.sbooth.AudioEngine.File.Musepack";

@implementation SFBMusepackFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"mpc"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/musepack", @"audio/x-musepack" ]];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameMusepack;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [fileHandle readHeaderOfLength:SFBMusepackDetectionSize skipID3v2Tag:YES error:error];
    if (header == nil) {
        return NO;
    }

    if ([header isMusepackHeader]) {
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

        TagLib::MPC::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Musepack", @"")];
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
                [NSMutableDictionary dictionaryWithObject:@"Musepack" forKey:SFBAudioPropertiesKeyFormatName];
        if (const auto *properties = file.audioProperties(); properties != nullptr) {
            sfb::addAudioPropertiesToDictionary(properties, propertiesDictionary);

            if (properties->sampleFrames() != 0) {
                propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(properties->sampleFrames());
            }
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (file.hasID3v1Tag()) {
            [metadata addMetadataFromTagLibID3v1Tag:file.ID3v1Tag()];
        }

        if (file.hasAPETag()) {
            [metadata addMetadataFromTagLibAPETag:file.APETag()];
        }

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception &e) {
        os_log_error(gSFBAudioFileLog, "Error reading Musepack properties and metadata: %{public}s", e.what());
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

        TagLib::MPC::File file(&stream, false);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Musepack", @"")];
            }
            return NO;
        }

        // ID3v1 tags are only written if present, but an APE tag is always written

        if (file.hasID3v1Tag()) {
            sfb::setID3v1TagFromMetadata(self.metadata, file.ID3v1Tag());
        }

        sfb::setAPETagFromMetadata(self.metadata, file.APETag(true));

        if (!file.save()) {
            if (error != nullptr) {
                *error = [self genericSaveError];
            }
            return NO;
        }

        return YES;
    } catch (const std::exception &e) {
        os_log_error(gSFBAudioFileLog, "Error writing Musepack metadata: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

@end
