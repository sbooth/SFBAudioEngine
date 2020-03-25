/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <os/log.h>

#import <taglib/itfile.h>
#import <taglib/tfilestream.h>

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBImpulseTrackerModuleMetadata.h"

@implementation SFBImpulseTrackerModuleMetadata

+ (void)load
{
	[SFBAudioMetadata registerInputOutputHandler:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"it"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/it"];
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

	TagLib::IT::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Impulse Tracker module file.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not an Impulse Tracker module file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return nil;
	}

	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	metadata.formatName = @"Impulse Tracker Module";

	if(file.audioProperties())
		[metadata addAudioPropertiesFromTagLibAudioProperties:file.audioProperties()];

	if(file.tag())
		[metadata addMetadataFromTagLibTag:file.tag()];

	return metadata;
}

- (BOOL)writeAudioMetadata:(SFBAudioMetadata *)metadata toURL:(NSURL *)url error:(NSError **)error
{
#pragma unused(metadata)
	os_log_error(OS_LOG_DEFAULT, "Writing Impulse Tracker module metadata is not supported");

	if(error)
		*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
										 code:SFBAudioMetadataErrorCodeInputOutput
				descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
										  url:url
								failureReason:NSLocalizedString(@"Unable to write metadata", @"")
						   recoverySuggestion:NSLocalizedString(@"Writing Impulse Tracker module metadata is not supported.", @"")];
	return NO;
}

@end
