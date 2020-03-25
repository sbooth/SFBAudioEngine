/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <taglib/mp4file.h>
#import <taglib/tfilestream.h>

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibMP4Tag.h"
#import "SFBMP4Metadata.h"

@implementation SFBMP4Metadata

+ (void)load
{
	[SFBAudioMetadata registerInputOutputHandler:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"m4a", @"m4r", @"mp4"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/mpeg-4"]];
}

- (SFBAudioMetadata *)readAudioMetadataFromURL:(NSURL *)url error:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(url.fileSystemRepresentation, true));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for reading.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return nil;
	}

	TagLib::MP4::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MPEG-4 file.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not a MPEG-4 file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return nil;
	}

	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	metadata.formatName = @"MP4";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[metadata addAudioPropertiesFromTagLibAudioProperties:properties];

		if(properties->bitsPerSample())
			metadata.bitsPerChannel = @(properties->bitsPerSample());
		switch(properties->codec()) {
			case TagLib::MP4::AudioProperties::AAC:
				metadata.formatName = @"AAC";
				break;
			case TagLib::MP4::AudioProperties::ALAC:
				metadata.formatName = @"Apple Lossless";
				break;
			default:
				break;
		}
	}

	if(file.tag())
		[metadata addMetadataFromTagLibMP4Tag:file.tag()];

	return metadata;
}

- (BOOL)writeAudioMetadata:(SFBAudioMetadata *)metadata toURL:(NSURL *)url error:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(url.fileSystemRepresentation));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for writing.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::MP4::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MPEG-4 file.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not a MPEG-4 file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	SFB::Audio::SetMP4TagFromMetadata(metadata, file.tag());

	if(!file.save()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Unable to write metadata", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}


	return YES;
}

@end
