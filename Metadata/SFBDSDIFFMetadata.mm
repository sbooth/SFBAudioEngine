/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <taglib/dsdifffile.h>
#import <taglib/tfilestream.h>

#import "SetID3v2TagFromMetadata.h"
#import "SetTagFromMetadata.h"

#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBDSDIFFMetadata.h"

@implementation SFBDSDIFFMetadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"dff"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/dff"];
}

+ (BOOL)_handlesFilesWithExtension:(NSString *)extension
{
	return [extension caseInsensitiveCompare:@"dff"] == NSOrderedSame;
}

+ (BOOL)_handlesMIMEType:(NSString *)mimeType
{
	return [mimeType caseInsensitiveCompare:@"audio/dff"] == NSOrderedSame;
}

- (BOOL)_readMetadata:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(self.url.fileSystemRepresentation, true));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for reading.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Input/output error", @"")
										  recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::DSDIFF::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid DSD Interchange file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not a DSD Interchange file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"DSD Interchange";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[self addAudioPropertiesFromTagLibAudioProperties:properties];

		if(properties->bitsPerSample())
			self.bitsPerChannel = @(properties->bitsPerSample());
		if(properties->sampleCount())
			self.totalFrames = @(properties->sampleCount());
	}

	if(file.hasDIINTag())
		[self addMetadataFromTagLibTag:file.DIINTag()];

	if(file.hasID3v2Tag())
		[self addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];

	return YES;
}

- (BOOL)_writeMetadata:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(self.url.fileSystemRepresentation));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for writing.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Input/output error", @"")
										  recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::DSDIFF::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid DSD Interchange file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not a DSD Interchange file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	SFB::Audio::SetTagFromMetadata(self, file.tag());
	SFB::Audio::SetID3v2TagFromMetadata(self, file.ID3v2Tag());

	if(!file.save()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Unable to write metadata", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}


	return YES;
}

@end
