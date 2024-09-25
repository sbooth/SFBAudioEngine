//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"

#import <taglib/aifffile.h>
#import <taglib/tfilestream.h>

#pragma clang diagnostic pop

#import "SFBAIFFFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSError+SFBURLPresentation.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameAIFF = @"org.sbooth.AudioEngine.File.AIFF";

@implementation SFBAIFFFile

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"aiff", @"aif"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/aiff"];
}

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameAIFF;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(fileHandle != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSData *header = [fileHandle readHeaderOfLength:SFBAIFFDetectionSize skipID3v2Tag:NO error:error];
	if(!header)
		return NO;

	if([header isAIFFHeader])
		*formatIsSupported = SFBTernaryTruthValueTrue;
	else
		*formatIsSupported = SFBTernaryTruthValueFalse;

	return YES;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	TagLib::FileStream stream(self.url.fileSystemRepresentation, true);
	if(!stream.isOpen()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for reading.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::RIFF::AIFF::File file(&stream);
	if(!file.isValid()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid AIFF file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not an AIFF file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"AIFF" forKey:SFBAudioPropertiesKeyFormatName];
	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		SFB::Audio::AddAudioPropertiesToDictionary(properties, propertiesDictionary);

		if(properties->bitsPerSample())
			propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
		if(properties->sampleFrames())
			propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(properties->sampleFrames());
	}

	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	if(file.tag())
		[metadata addMetadataFromTagLibID3v2Tag:file.tag()];

	self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
	self.metadata = metadata;
	return YES;
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	TagLib::FileStream stream(self.url.fileSystemRepresentation);
	if(!stream.isOpen()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for writing.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::RIFF::AIFF::File file(&stream, false);
	if(!file.isValid()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid AIFF file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not an AIFF file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	SFB::Audio::SetID3v2TagFromMetadata(self.metadata, file.tag());

	if(!file.save()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Unable to write metadata", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	return YES;
}

@end
