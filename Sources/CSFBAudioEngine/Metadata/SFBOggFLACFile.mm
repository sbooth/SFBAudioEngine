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
    if (header == nil) {
        return NO;
    }

    if ([header isOggFLACHeader]) {
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

        TagLib::Ogg::FLAC::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Ogg FLAC", @"")];
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
              [NSMutableDictionary dictionaryWithObject:@"Ogg FLAC" forKey:SFBAudioPropertiesKeyFormatName];
        if (const auto *properties = file.audioProperties(); properties != nullptr) {
            sfb::addAudioPropertiesToDictionary(properties, propertiesDictionary);

            if (properties->bitsPerSample() != 0) {
                propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
            }
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (const auto *tag = file.tag(); tag != nullptr) {
            [metadata addMetadataFromTagLibXiphComment:tag];
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
                *error = [self genericOpenForWritingError];
            }
            return NO;
        }

        TagLib::Ogg::FLAC::File file(&stream, false);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Ogg FLAC", @"")];
            }
            return NO;
        }

        sfb::setXiphCommentFromMetadata(self.metadata, file.tag());

        if (!file.save()) {
            if (error != nullptr) {
                *error = [self genericSaveError];
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
