//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBProTrackerModuleFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/modfile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameProTrackerModule = @"org.sbooth.AudioEngine.File.ProTrackerModule";

@implementation SFBProTrackerModuleFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"mod"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/mod", @"audio/x-mod" ]];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameProTrackerModule;
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

        TagLib::Mod::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"ProTracker module", @"")];
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"ProTracker Module" forKey:SFBAudioPropertiesKeyFormatName];
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
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error reading ProTracker module properties and metadata: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

- (BOOL)writeMetadataReturningError:(NSError **)error {
    os_log_error(gSFBAudioFileLog, "Writing ProTracker module metadata is not supported");
    if (error != nullptr) {
        *error = [self
              saveErrorWithRecoverySuggestion:NSLocalizedString(@"Writing ProTracker module metadata is not supported.",
                                                                @"")];
    }
    return NO;
}

@end
