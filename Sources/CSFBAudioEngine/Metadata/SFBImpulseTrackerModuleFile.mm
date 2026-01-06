//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/itfile.h>
#import <taglib/tfilestream.h>

#import "SFBImpulseTrackerModuleFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameImpulseTrackerModule = @"org.sbooth.AudioEngine.File.ImpulseTrackerModule";

@implementation SFBImpulseTrackerModuleFile

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"it"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/it"];
}

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameImpulseTrackerModule;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(fileHandle != nil);
	NSParameterAssert(formatIsSupported != NULL);

	*formatIsSupported = SFBTernaryTruthValueUnknown;

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

		TagLib::IT::File file(&stream);
		if(!file.isValid()) {
			if(error)
				*error = SFBErrorWithLocalizedDescription(SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
														  NSLocalizedString(@"The file “%@” is not a valid Impulse Tracker module file.", @""),
														  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's extension may not match the file's type.", @""),
															 NSURLErrorKey: self.url },
														  SFBLocalizedNameForURL(self.url));
			return NO;
		}

		NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"Impulse Tracker Module" forKey:SFBAudioPropertiesKeyFormatName];
		if(file.audioProperties())
			SFB::Audio::AddAudioPropertiesToDictionary(file.audioProperties(), propertiesDictionary);

		SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
		if(file.tag())
			[metadata addMetadataFromTagLibTag:file.tag()];

		self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
		self.metadata = metadata;

		return YES;
	} catch(const std::exception& e) {
		os_log_error(gSFBAudioFileLog, "Error reading Impulse Tracker module properties and metadata: %{public}s", e.what());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioFileErrorDomain code:SFBAudioFileErrorCodeInternalError userInfo:nil];
		return NO;
	}
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	os_log_error(gSFBAudioFileLog, "Writing Impulse Tracker module metadata is not supported");

	if(error)
		*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
										 code:SFBAudioFileErrorCodeInputOutput
				descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
										  url:self.url
								failureReason:NSLocalizedString(@"Unable to write metadata", @"")
						   recoverySuggestion:NSLocalizedString(@"Writing Impulse Tracker module metadata is not supported.", @"")];
	return NO;
}

@end
