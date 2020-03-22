/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <taglib/mp4file.h>
#import <taglib/tfilestream.h>

#import "SetMP4TagFromMetadata.h"

#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibMP4Tag.h"
#import "SFBMP4Metadata.h"

@implementation SFBMP4Metadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"m4a", @"m4r", @"mp4"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/mpeg-4"];
}

+ (BOOL)_handlesFilesWithExtension:(NSString *)extension
{
	return [extension caseInsensitiveCompare:@"m4a"] == NSOrderedSame || [extension caseInsensitiveCompare:@"m4r"] == NSOrderedSame || [extension caseInsensitiveCompare:@"mp4"] == NSOrderedSame;
}

+ (BOOL)_handlesMIMEType:(NSString *)mimeType
{
	return [mimeType caseInsensitiveCompare:@"audio/mpeg-4"] == NSOrderedSame;
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

	TagLib::MP4::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MPEG-4 file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not a MPEG-4 file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"MP4";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[self addAudioPropertiesFromTagLibAudioProperties:properties];

		if(properties->bitsPerSample())
			self.bitsPerChannel = @(properties->bitsPerSample());
		switch(properties->codec()) {
			case TagLib::MP4::AudioProperties::AAC:
				self.formatName = @"AAC";
				break;
			case TagLib::MP4::AudioProperties::ALAC:
				self.formatName = @"Apple Lossless";
				break;
			default:
				break;
		}
	}

	if(file.tag())
		[self addMetadataFromTagLibMP4Tag:file.tag()];

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

	TagLib::MP4::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MPEG-4 file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not a MPEG-4 file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	SFB::Audio::SetMP4TagFromMetadata(self, file.tag());

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
