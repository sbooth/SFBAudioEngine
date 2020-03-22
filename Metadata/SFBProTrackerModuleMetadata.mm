/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <os/log.h>

#import <taglib/modfile.h>
#import <taglib/tfilestream.h>

#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBProTrackerModuleMetadata.h"

@implementation SFBProTrackerModuleMetadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"mod"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/mod", @"audio/x-mod"];
}

+ (BOOL)_handlesFilesWithExtension:(NSString *)extension
{
	return [extension caseInsensitiveCompare:@"mod"] == NSOrderedSame;
}

+ (BOOL)_handlesMIMEType:(NSString *)mimeType
{
	return [mimeType caseInsensitiveCompare:@"audio/mod"] == NSOrderedSame || [mimeType caseInsensitiveCompare:@"audio/x-mod"] == NSOrderedSame;
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

	TagLib::Mod::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid ProTracker module file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not a ProTracker module file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"ProTracker Module";

	if(file.audioProperties())
		[self addAudioPropertiesFromTagLibAudioProperties:file.audioProperties()];

	if(file.tag())
		[self addMetadataFromTagLibTag:file.tag()];

	return YES;
}

- (BOOL)_writeMetadata:(NSError **)error
{
	os_log_error(OS_LOG_DEFAULT, "Writing ProTracker module metadata is not supported");

	if(error)
		*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
						   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
													 url:self.url
										   failureReason:NSLocalizedString(@"Unable to write metadata", @"")
									  recoverySuggestion:NSLocalizedString(@"Writing ProTracker module metadata is not supported.", @"")];
	return NO;
}

@end
