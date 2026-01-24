//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBOggFLACFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/oggflacfile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameOggFLAC = @"org.sbooth.AudioEngine.File.OggFLAC";

@implementation SFBOggFLACFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"oga"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=flac"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameOggFLAC;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [fileHandle readHeaderOfLength:SFBOggFLACDetectionSize skipID3v2Tag:NO error:error];
    if (header == nullptr) {
        return NO;
    }

    if ([header isOggFLACHeader] != NO) {
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

        TagLib::Ogg::FLAC::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid Ogg FLAC file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"Ogg FLAC" forKey:SFBAudioPropertiesKeyFormatName];
        if (file.audioProperties() != nullptr) {
            auto *properties = file.audioProperties();
            sfb::addAudioPropertiesToDictionary(properties, propertiesDictionary);

            if (properties->bitsPerSample() != 0) {
                propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
            }
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (file.tag() != nullptr) {
            [metadata addMetadataFromTagLibXiphComment:file.tag()];
        }

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error reading Ogg FLAC properties and metadata: %{public}s", e.what());
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
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
                      NSLocalizedString(@"The file “%@” could not be opened for writing.", @""), @{
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

        TagLib::Ogg::FLAC::File file(&stream, false);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid Ogg FLAC file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        sfb::setXiphCommentFromMetadata(self.metadata, file.tag());

        if (!file.save()) {
            if (error != nullptr) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
                      NSLocalizedString(@"The file “%@” could not be saved.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        return YES;
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error writing Ogg FLAC metadata: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

@end
