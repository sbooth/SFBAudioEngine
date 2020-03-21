/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <taglib/tfilestream.h>
#import <taglib/wavpackfile.h>

#import "SetAPETagFromMetadata.h"
#import "SetID3v1TagFromMetadata.h"

#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAPETag.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBWavPackMetadata.h"

@implementation SFBWavPackMetadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"wv"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/wavpack"];
}

+ (BOOL)_handlesFilesWithExtension:(NSString *)extension
{
	return [extension caseInsensitiveCompare:@"wv"] == NSOrderedSame;
}

+ (BOOL)_handlesMIMEType:(NSString *)mimeType
{
	return [mimeType caseInsensitiveCompare:@"audio/wavpack"] == NSOrderedSame;
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

	TagLib::WavPack::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WavPack file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not a WavPack file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"WavPack";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[self addAudioPropertiesFromTagLibAudioProperties:properties];

		if(properties->bitsPerSample())
			self.bitsPerChannel = @(properties->bitsPerSample());
		if(properties->sampleFrames())
			self.totalFrames = @(properties->sampleFrames());
	}

	if(file.hasID3v1Tag())
		[self addMetadataFromTagLibID3v1Tag:file.ID3v1Tag()];

	if(file.hasAPETag())
		[self addMetadataFromTagLibAPETag:file.APETag()];

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

	TagLib::WavPack::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WavPack file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not a WavPack file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// ID3v1 tags are only written if present, but an APE tag is always written

	if(file.hasID3v1Tag())
		SFB::Audio::SetID3v1TagFromMetadata(self, file.ID3v1Tag());

	SFB::Audio::SetAPETagFromMetadata(self, file.APETag(true));

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
