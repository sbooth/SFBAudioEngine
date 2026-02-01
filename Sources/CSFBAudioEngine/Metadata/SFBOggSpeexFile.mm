//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBOggSpeexFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "SFBLocalizedNameForURL.h"

#import <taglib/speexfile.h>
#import <taglib/tfilestream.h>

SFBAudioFileFormatName const SFBAudioFileFormatNameOggSpeex = @"org.sbooth.AudioEngine.File.OggSpeex";

@implementation SFBOggSpeexFile

+ (void)load {
    [SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"spx"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/ogg; codecs=speex"];
}

+ (SFBAudioFileFormatName)formatName {
    return SFBAudioFileFormatNameOggSpeex;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error {
    NSParameterAssert(fileHandle != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [fileHandle readHeaderOfLength:SFBOggSpeexDetectionSize skipID3v2Tag:NO error:error];
    if (header == nil) {
        return NO;
    }

    if ([header isOggSpeexHeader]) {
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

        TagLib::Ogg::Speex::File file(&stream);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Ogg Speex", @"")];
            }
            return NO;
        }

        NSMutableDictionary *propertiesDictionary =
                [NSMutableDictionary dictionaryWithObject:@"Ogg Speex" forKey:SFBAudioPropertiesKeyFormatName];
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
        os_log_error(gSFBAudioFileLog, "Error reading Ogg Speex properties and metadata: %{public}s", e.what());
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

        TagLib::Ogg::Speex::File file(&stream, false);
        if (!file.isValid()) {
            if (error != nullptr) {
                *error = [self genericInvalidFormatError:NSLocalizedString(@"Ogg Speex", @"")];
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
        os_log_error(gSFBAudioFileLog, "Error writing Ogg Speex metadata: %{public}s", e.what());
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }
}

@end
