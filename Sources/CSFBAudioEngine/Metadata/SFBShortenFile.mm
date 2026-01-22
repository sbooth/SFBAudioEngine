//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBShortenFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/shortenfile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameShorten = @"org.sbooth.AudioEngine.File.Shorten";

@implementation SFBShortenFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"shn"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/x-shorten"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameShorten;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != NULL);

    NSData *header = [fileHandle readHeaderOfLength:SFBShortenDetectionSize skipID3v2Tag:NO error:error];
    if (!header) {
        return NO;
    }

    if ([header isShortenHeader])
        *formatIsSupported = SFBTernaryTruthValueTrue;
    else
        *formatIsSupported = SFBTernaryTruthValueFalse;

    return YES;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error {
    try {
        TagLib::FileStream stream(self.url.fileSystemRepresentation, true);
        if (!stream.isOpen()) {
            if (error) {
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
            }
            return NO;
        }

        TagLib::Shorten::File file(&stream);
        if (!file.isValid()) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"Shorten" forKey:SFBAudioPropertiesKeyFormatName];
        if (file.audioProperties()) {
            sfb::addAudioPropertiesToDictionary(file.audioProperties(), propertiesDictionary);
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (file.tag())
            [metadata addMetadataFromTagLibTag:file.tag()];

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error reading Shorten properties and metadata: %{public}s", e.what());
        if (error)
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        return NO;
    }
}

- (BOOL)writeMetadataReturningError:(NSError **)error {
    os_log_error(gSFBAudioFileLog, "Writing Shorten metadata is not supported");
    if (error) {
        *error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
                                                  NSLocalizedString(@"The file “%@” could not be saved.", @""), @{
                                                      NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                                            @"Writing Shorten metadata is not supported.", @""),
                                                      NSURLErrorKey : self.url
                                                  },
                                                  SFBLocalizedNameForURL(self.url));
    }
    return NO;
}

@end
