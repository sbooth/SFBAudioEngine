//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/mp4file.h>
#import <taglib/tfilestream.h>

#import "SFBMP4File.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibMP4Tag.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameMP4 = @"org.sbooth.AudioEngine.File.MP4";

@implementation SFBMP4File

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"m4a", @"m4r", @"mp4"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/mpeg-4"];
}

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameMP4;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(fileHandle != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSData *header = [fileHandle readHeaderOfLength:SFBMPEG4DetectionSize skipID3v2Tag:NO error:error];
	if(!header)
		return NO;

	if([header isMPEG4Header])
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

		TagLib::MP4::File file(&stream);
		if(!file.isValid()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
														  NSLocalizedString(@"The file “%@” is not a valid MPEG-4 file.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's extension may not match the file's type.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"MP4" forKey:SFBAudioPropertiesKeyFormatName];
		if(file.audioProperties()) {
			auto properties = file.audioProperties();
			SFB::Audio::AddAudioPropertiesToDictionary(properties, propertiesDictionary);

			if(properties->bitsPerSample())
				propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
			switch(properties->codec()) {
				case TagLib::MP4::Properties::Codec::AAC:
					propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"AAC";
					break;
				case TagLib::MP4::Properties::Codec::ALAC:
					propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"Apple Lossless";
					break;
				default:
					break;
			}
		}

		SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
		if(file.tag())
			[metadata addMetadataFromTagLibMP4Tag:file.tag()];

		self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
		self.metadata = metadata;

		return YES;
	} catch(const std::exception& e) {
		os_log_error(gSFBAudioFileLog, "Error reading MPEG-4 properties and metadata: %{public}s", e.what());
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

		TagLib::MP4::File file(&stream, false);
		if(!file.isValid()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
														  NSLocalizedString(@"The file “%@” is not a valid MPEG-4 file.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's extension may not match the file's type.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		SFB::Audio::SetMP4TagFromMetadata(self.metadata, file.tag());

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
		os_log_error(gSFBAudioFileLog, "Error writing MPEG-4 metadata: %{public}s", e.what());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioFileErrorDomain code:SFBAudioFileErrorCodeInternalError userInfo:nil];
		return NO;
	}
}

@end
