//
// Copyright (c) 2006 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"

#import <taglib/tfilestream.h>
#import <taglib/wavfile.h>

#pragma clang diagnostic pop

#import "SFBWAVEFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameWAVE = @"org.sbooth.AudioEngine.File.WAVE";

@implementation SFBWAVEFile

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"wav", @"wave"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/wave"];
}

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameWAVE;
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

	TagLib::RIFF::WAV::File file(&stream);
	if(!file.isValid()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WAVE file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not an WAVE file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"WAVE" forKey:SFBAudioPropertiesKeyFormatName];
	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		SFB::Audio::AddAudioPropertiesToDictionary(properties, propertiesDictionary);

		if(properties->bitsPerSample())
			propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
		if(properties->sampleFrames())
			propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(properties->sampleFrames());
	}

	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	if(file.hasInfoTag())
		[metadata addMetadataFromTagLibTag:file.InfoTag()];

	if(file.hasID3v2Tag())
		[metadata addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];

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

	TagLib::RIFF::WAV::File file(&stream, false);
	if(!file.isValid()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WAVE file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not a WAVE file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// An Info tag is only written if present, but ID3v2 tags are always written

	// TODO: Should other field names from the Info tag be handled?
	if(file.hasInfoTag())
		SFB::Audio::SetTagFromMetadata(self.metadata, file.InfoTag());

	SFB::Audio::SetID3v2TagFromMetadata(self.metadata, file.ID3v2Tag());

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
