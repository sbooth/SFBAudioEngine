//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBImpulseTrackerModuleFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/itfile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameImpulseTrackerModule =
        @"org.sbooth.AudioEngine.File.ImpulseTrackerModule";

@implementation SFBImpulseTrackerModuleFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"it"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/it"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameImpulseTrackerModule;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    *formatIsSupported = SFBTernaryTruthValueUnknown;

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

        TagLib::IT::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Impulse Tracker module", @"")];
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
                [NSMutableDictionary dictionaryWithObject:@"Impulse Tracker Module"
                                                   forKey:SFBAudioPropertiesKeyFormatName];
        if (file.audioProperties() != nullptr) {
            sfb::addAudioPropertiesToDictionary(file.audioProperties(), propertiesDictionary);
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (const auto *tag = file.tag(); tag != nullptr) {
            [metadata addMetadataFromTagLibTag:tag];
        }

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception &e) {
        os_log_error(gSFBAudioFileLog, "Error reading Impulse Tracker module properties and metadata: %{public}s",
                     e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

- (BOOL)writeMetadataReturningError:(NSError **)error {
    os_log_error(gSFBAudioFileLog, "Writing Impulse Tracker module metadata is not supported");
    if (error != nullptr) {
        *error = [self
                saveErrorWithRecoverySuggestion:NSLocalizedString(
                                                        @"Writing Impulse Tracker module metadata is not supported.",
                                                        @"")];
    }
    return NO;
}

@end
