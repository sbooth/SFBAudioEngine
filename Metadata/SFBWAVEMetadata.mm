/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <taglib/tfilestream.h>
#import <taglib/wavfile.h>

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBWAVEMetadata.h"

@implementation SFBWAVEMetadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"wav", @"wave"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/wave"];
}

- (BOOL)_readMetadata:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(self.url.fileSystemRepresentation, true));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for reading.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::RIFF::WAV::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WAVE file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not an WAVE file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"WAVE";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[self addAudioPropertiesFromTagLibAudioProperties:properties];

		if(properties->sampleWidth())
			self.bitsPerChannel = @(properties->sampleWidth());
		if(properties->sampleFrames())
			self.totalFrames = @(properties->sampleFrames());
	}

	if(file.hasInfoTag())
		[self addMetadataFromTagLibTag:file.InfoTag()];

	if(file.hasID3v2Tag())
		[self addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];

	return YES;
}

- (BOOL)_writeMetadata:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(self.url.fileSystemRepresentation));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for writing.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::RIFF::WAV::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WAVE file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not a WAVE file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// An Info tag is only written if present, but ID3v2 tags are always written

	// TODO: Should other field names from the Info tag be handled?
	if(file.hasInfoTag())
		SFB::Audio::SetTagFromMetadata(self, file.InfoTag());

	SFB::Audio::SetID3v2TagFromMetadata(self, file.ID3v2Tag());

	if(!file.save()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Unable to write metadata", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	return YES;
}

@end
