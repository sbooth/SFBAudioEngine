//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBMP3File.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibAPETag.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/mpegfile.h>
#import <taglib/tfilestream.h>
#import <taglib/xingheader.h>

#import <memory>

SFBAudioFileFormatName const SFBAudioFileFormatNameMP3 = @"org.sbooth.AudioEngine.File.MP3";

@implementation SFBMP3File

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"mp3"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/mpeg"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameMP3;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != NULL);

    NSData *header = [fileHandle readHeaderOfLength:SFBMP3DetectionSize skipID3v2Tag:YES error:error];
    if (!header) {
        return NO;
    }

    if ([header isMP3Header]) {
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

        TagLib::MPEG::File file(&stream);
        if (!file.isValid()) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid MPEG file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"MP3" forKey:SFBAudioPropertiesKeyFormatName];
        if (file.audioProperties()) {
            auto *properties = file.audioProperties();
            sfb::addAudioPropertiesToDictionary(properties, propertiesDictionary);

            // TODO: Is this too much information?
#if 0
			switch(properties->version()) {
				case TagLib::MPEG::Header::Version1:
					switch(properties->layer()) {
						case 1: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-1 Layer I";		break;
						case 2: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-1 Layer II";		break;
						case 3: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-1 Layer III";	break;
					}
					break;
				case TagLib::MPEG::Header::Version2:
					switch(properties->layer()) {
						case 1: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-2 Layer I";		break;
						case 2: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-2 Layer II";		break;
						case 3: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-2 Layer III";	break;
					}
					break;
				case TagLib::MPEG::Header::Version2_5:
					switch(properties->layer()) {
						case 1: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-2.5 Layer I";	break;
						case 2: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-2.5 Layer II";	break;
						case 3: propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"MPEG-2.5 Layer III";	break;
					}
					break;
			}
#endif

            if (properties->xingHeader() && properties->xingHeader()->totalFrames()) {
                propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(properties->xingHeader()->totalFrames());
            }
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (file.hasAPETag()) {
            [metadata addMetadataFromTagLibAPETag:file.APETag()];
        }

        if (file.hasID3v1Tag()) {
            [metadata addMetadataFromTagLibID3v1Tag:file.ID3v1Tag()];
        }

        if (file.hasID3v2Tag()) {
            [metadata addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];
        }

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception& e) {
        os_log_error(gSFBAudioFileLog, "Error reading MPEG properties and metadata: %{public}s", e.what());
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

        TagLib::MPEG::File file(&stream, false);
        if (!file.isValid()) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                      NSLocalizedString(@"The file “%@” is not a valid MPEG file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                          NSURLErrorKey : self.url
                      },
                      SFBLocalizedNameForURL(self.url));
            }
            return NO;
        }

        // APE and ID3v1 tags are only written if present, but ID3v2 tags are always written

        if (file.hasAPETag()) {
            sfb::setAPETagFromMetadata(self.metadata, file.APETag());
        }

        if (file.hasID3v1Tag()) {
            sfb::setID3v1TagFromMetadata(self.metadata, file.ID3v1Tag());
        }

        sfb::setID3v2TagFromMetadata(self.metadata, file.ID3v2Tag(true));

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
        os_log_error(gSFBAudioFileLog, "Error writing MPEG metadata: %{public}s", e.what());
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

@end
