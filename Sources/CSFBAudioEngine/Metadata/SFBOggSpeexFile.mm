//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/speexfile.h>
#import <taglib/tfilestream.h>

#import "SFBOggSpeexFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameOggSpeex = @"org.sbooth.AudioEngine.File.OggSpeex";

@implementation SFBOggSpeexFile

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"spx"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/ogg; codecs=speex"];
}

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameOggSpeex;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(fileHandle != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSData *header = [fileHandle readHeaderOfLength:SFBOggSpeexDetectionSize skipID3v2Tag:NO error:error];
	if(!header)
		return NO;

	if([header isOggSpeexHeader])
		*formatIsSupported = SFBTernaryTruthValueTrue;
	else
		*formatIsSupported = SFBTernaryTruthValueFalse;

	return YES;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	try {
		TagLib::FileStream stream(self.url.fileSystemRepresentation, true);
		if(!stream.isOpen()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
														  NSLocalizedString(@"The file “%@” could not be opened for reading.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		TagLib::Ogg::Speex::File file(&stream);
		if(!file.isValid()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
														  NSLocalizedString(@"The file “%@” is not a valid Ogg Speex file.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's extension may not match the file's type.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"Ogg Speex" forKey:SFBAudioPropertiesKeyFormatName];
		if(file.audioProperties())
			SFB::Audio::AddAudioPropertiesToDictionary(file.audioProperties(), propertiesDictionary);

		SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
		if(file.tag())
			[metadata addMetadataFromTagLibXiphComment:file.tag()];

		self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
		self.metadata = metadata;

		return YES;
	} catch(const std::exception& e) {
		os_log_error(gSFBAudioFileLog, "Error reading Ogg Speex properties and metadata: %{public}s", e.what());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioFileErrorDomain code:SFBAudioFileErrorCodeInternalError userInfo:nil];
		return NO;
	}
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	try {
		TagLib::FileStream stream(self.url.fileSystemRepresentation);
		if(!stream.isOpen()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
														  NSLocalizedString(@"The file “%@” could not be opened for writing.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		TagLib::Ogg::Speex::File file(&stream, false);
		if(!file.isValid()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
														  NSLocalizedString(@"The file “%@” is not a valid Ogg Speex file.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's extension may not match the file's type.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		SFB::Audio::SetXiphCommentFromMetadata(self.metadata, file.tag());

		if(!file.save()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInputOutput,
														  NSLocalizedString(@"The file “%@” could not be saved.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's extension may not match the file's type.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		return YES;
	} catch(const std::exception& e) {
		os_log_error(gSFBAudioFileLog, "Error writing Ogg Speex metadata: %{public}s", e.what());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioFileErrorDomain code:SFBAudioFileErrorCodeInternalError userInfo:nil];
		return NO;
	}
}

@end
