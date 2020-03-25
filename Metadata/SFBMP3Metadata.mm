/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <taglib/mpegfile.h>
#import <taglib/tfilestream.h>
#import <taglib/xingheader.h>

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAPETag.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBMP3Metadata.h"

@implementation SFBMP3Metadata

+ (void)load
{
	[SFBAudioMetadata registerInputOutputHandler:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"mp3"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/mpeg"];
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

	TagLib::MPEG::File file(stream.get(), TagLib::ID3v2::FrameFactory::instance());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MPEG file.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not an MPEG file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return nil;
	}

	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	metadata.formatName = @"MP3";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[metadata addAudioPropertiesFromTagLibAudioProperties:properties];

		// TODO: Is this too much information?
#if 0
		switch(properties->version()) {
			case TagLib::MPEG::Header::Version1:
				switch(properties->layer()) {
					case 1:		metadata.formatName = @"MPEG-1 Layer I";		break;
					case 2:		metadata.formatName = @"MPEG-1 Layer II";		break;
					case 3:		metadata.formatName = @"MPEG-1 Layer III";		break;
				}
				break;
			case TagLib::MPEG::Header::Version2:
				switch(properties->layer()) {
					case 1:		metadata.formatName = @"MPEG-2 Layer I";		break;
					case 2:		metadata.formatName = @"MPEG-2 Layer II";		break;
					case 3:		metadata.formatName = @"MPEG-2 Layer III";		break;
				}
				break;
			case TagLib::MPEG::Header::Version2_5:
				switch(properties->layer()) {
					case 1:		metadata.formatName = @"MPEG-2.5 Layer I";		break;
					case 2:		metadata.formatName = @"MPEG-2.5 Layer II";		break;
					case 3:		metadata.formatName = @"MPEG-2.5 Layer III";	break;
				}
				break;
		}
#endif

		if(properties->xingHeader() && properties->xingHeader()->totalFrames())
			metadata.totalFrames = @(properties->xingHeader()->totalFrames());
	}

	if(file.hasAPETag())
		[metadata addMetadataFromTagLibAPETag:file.APETag()];

	if(file.hasID3v1Tag())
		[metadata addMetadataFromTagLibID3v1Tag:file.ID3v1Tag()];

	if(file.hasID3v2Tag())
		[metadata addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];

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

	TagLib::MPEG::File file(stream.get(), TagLib::ID3v2::FrameFactory::instance(), false);
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MPEG file.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not an MPEG file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// APE and ID3v1 tags are only written if present, but ID3v2 tags are always written

	if(file.hasAPETag())
		SFB::Audio::SetAPETagFromMetadata(metadata, file.APETag());

	if(file.hasID3v1Tag())
		SFB::Audio::SetID3v1TagFromMetadata(metadata, file.ID3v1Tag());

	SFB::Audio::SetID3v2TagFromMetadata(metadata, file.ID3v2Tag(true));

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
