//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBOggVorbisFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/tfilestream.h>
#import <taglib/vorbisfile.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameOggVorbis = @"org.sbooth.AudioEngine.File.OggVorbis";

@implementation SFBOggVorbisFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithArray:@[ @"ogg", @"oga" ]];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=vorbis"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameOggVorbis;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [fileHandle readHeaderOfLength:SFBOggVorbisDetectionSize skipID3v2Tag:NO error:error];
    if (header == nil) {
        return NO;
    }

    if ([header isOggVorbisHeader]) {
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

        TagLib::Ogg::Vorbis::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Ogg Vorbis", @"")];
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
                [NSMutableDictionary dictionaryWithObject:@"Ogg Vorbis" forKey:SFBAudioPropertiesKeyFormatName];
        if (file.audioProperties() != nullptr) {
            sfb::addAudioPropertiesToDictionary(file.audioProperties(), propertiesDictionary);
        }

        SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
        if (const auto *tag = file.tag(); tag != nullptr) {
            [metadata addMetadataFromTagLibXiphComment:tag];
        }

        self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
        self.metadata = metadata;

        return YES;
    } catch (const std::exception &e) {
        os_log_error(gSFBAudioFileLog, "Error reading Ogg Vorbis properties and metadata: %{public}s", e.what());
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

        TagLib::Ogg::Vorbis::File file(&stream, false);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Ogg Vorbis", @"")];
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
    } catch (const std::exception &e) {
        os_log_error(gSFBAudioFileLog, "Error writing Ogg Vorbis metadata: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

@end
