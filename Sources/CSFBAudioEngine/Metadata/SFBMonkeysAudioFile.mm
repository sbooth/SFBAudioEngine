//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBMonkeysAudioFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibAPETag.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/apefile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameMonkeysAudio = @"org.sbooth.AudioEngine.File.MonkeysAudio";

@implementation SFBMonkeysAudioFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"ape"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/monkeys-audio", @"audio/x-monkeys-audio" ]];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameMonkeysAudio;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != NULL);

    NSData *header = [fileHandle readHeaderOfLength:SFBAPEDetectionSize skipID3v2Tag:YES error:error];
    if (!header) {
        return NO;
    }

    if ([header isAPEHeader]) {
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

        TagLib::APE::File file(&stream);
        if (!file.isValid()) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid Monkey's Audio file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"Monkey's Audio" forKey:SFBAudioPropertiesKeyFormatName];
        if (file.audioProperties()) {
            auto properties = file.audioProperties();
            sfb::addAudioPropertiesToDictionary(properties, propertiesDictionary);

            if (properties->bitsPerSample()) {
                propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
            }
            if (properties->sampleFrames()) {
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
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error reading Monkey's Audio properties and metadata: %{public}s", e.what());
        if (error) {
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
            if (error) {
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

        TagLib::APE::File file(&stream, false);
        if (!file.isValid()) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid Monkey's Audio file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        // ID3v1 tags are only written if present, but an APE tag is always written

        if (file.hasID3v1Tag()) {
            sfb::setID3v1TagFromMetadata(self.metadata, file.ID3v1Tag());
        }

        sfb::setAPETagFromMetadata(self.metadata, file.APETag(true));

        if (!file.save()) {
            if (error) {
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
        os_log_error(gSFBAudioFileLog, "Error writing Monkey's Audio metadata: %{public}s", e.what());
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

@end
