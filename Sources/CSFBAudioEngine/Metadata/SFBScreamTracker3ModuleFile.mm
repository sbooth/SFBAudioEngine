//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBScreamTracker3ModuleFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/s3mfile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameScreamTracker3Module =
      @"org.sbooth.AudioEngine.File.ScreamTracker3Module";

@implementation SFBScreamTracker3ModuleFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"s3m"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/s3m"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameScreamTracker3Module;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != NULL);

    *formatIsSupported = SFBTernaryTruthValueUnknown;

    return YES;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error {
    try {
        TagLib::FileStream stream(self.url.fileSystemRepresentation, true);
        if (!stream.isOpen()) {
            if (error)
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
                      NSLocalizedString(@"The file “%@” could not be opened for reading.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not "
                                                  @"have appropriate permissions.",
                                                  @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            return NO;
        }

        TagLib::S3M::File file(&stream);
        if (!file.isValid()) {
            if (error)
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid Scream Tracker 3 module.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"Scream Tracker 3 Module"
                                                 forKey:SFBAudioPropertiesKeyFormatName];
        if (file.audioProperties())
            sfb::addAudioPropertiesToDictionary(file.audioProperties(), propertiesDictionary);

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (file.tag())
            [metadata addMetadataFromTagLibTag:file.tag()];

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error reading Scream Tracker 3 module properties and metadata: %{public}s",
                     e.what());
        if (error)
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        return NO;
    }
}

- (BOOL)writeMetadataReturningError:(NSError **)error {
    os_log_error(gSFBAudioFileLog, "Writing Scream Tracker 3 module metadata is not supported");
    if (error)
        *error = SFBErrorWithLocalizedDescription(
              SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
              NSLocalizedString(@"The file “%@” could not be saved.", @""), @{
                  NSLocalizedRecoverySuggestionErrorKey :
                        NSLocalizedString(@"Writing Scream Tracker 3 metadata is not supported.", @""),
                  NSURLErrorKey : self.url
              },
              SFBLocalizedNameForURL(self.url));
    return NO;
}

@end
