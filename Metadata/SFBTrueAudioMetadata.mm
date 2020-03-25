/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <taglib/tfilestream.h>
#import <taglib/trueaudiofile.h>

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBTrueAudioMetadata.h"

@implementation SFBTrueAudioMetadata

+ (void)load
{
	[SFBAudioMetadata registerInputOutputHandler:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"tta"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/x-tta"];
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

	TagLib::TrueAudio::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid True Audio file.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not a True Audio file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return nil;
	}

	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	metadata.formatName = @"True Audio";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[metadata addAudioPropertiesFromTagLibAudioProperties:properties];

		if(properties->bitsPerSample())
			metadata.bitsPerChannel = @(properties->bitsPerSample());
		if(properties->sampleFrames())
			metadata.totalFrames = @(properties->sampleFrames());
	}

	// Add all tags that are present
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

	TagLib::TrueAudio::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid True Audio file.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not a True Audio file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// ID3v1 tags are only written if present, but ID3v2 tags are always written

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
