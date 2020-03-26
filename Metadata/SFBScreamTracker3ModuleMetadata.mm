/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <os/log.h>

#import <taglib/s3mfile.h>
#import <taglib/tfilestream.h>

#import "AddAudioPropertiesToDictionary.h"
#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBAudioProperties.h"
#import "SFBScreamTracker3ModuleMetadata.h"

@implementation SFBScreamTracker3ModuleMetadata

+ (void)load
{
	[SFBAudioFile registerInputOutputHandler:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"s3m"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/s3m"];
}

- (BOOL)readAudioPropertiesAndMetadataFromURL:(NSURL *)url toAudioFile:(SFBAudioFile *)audioFile error:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(url.fileSystemRepresentation, true));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for reading.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::S3M::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Scream Tracker 3 module.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Not a Scream Tracker 3 module", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"Scream Tracker 3 Module" forKey:SFBAudioPropertiesFormatNameKey];
	if(file.audioProperties())
		SFB::Audio::AddAudioPropertiesToDictionary(file.audioProperties(), propertiesDictionary);

	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	if(file.tag())
		[metadata addMetadataFromTagLibTag:file.tag()];

	audioFile.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
	audioFile.metadata = metadata;
	return YES;
}

- (BOOL)writeAudioMetadata:(SFBAudioMetadata *)metadata toURL:(NSURL *)url error:(NSError **)error
{
#pragma unused(metadata)
	os_log_error(OS_LOG_DEFAULT, "Writing Scream Tracker 3 module metadata is not supported");

	if(error)
		*error = [NSError sfb_errorWithDomain:SFBAudioFileErrorDomain
										 code:SFBAudioFileErrorCodeInputOutput
				descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
										  url:url
								failureReason:NSLocalizedString(@"Unable to write metadata", @"")
						   recoverySuggestion:NSLocalizedString(@"Writing Scream Tracker 3 module metadata is not supported.", @"")];
	return NO;
}

@end
