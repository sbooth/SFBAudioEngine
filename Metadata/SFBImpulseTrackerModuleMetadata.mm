/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <os/log.h>

#import <taglib/itfile.h>
#import <taglib/tfilestream.h>

#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBImpulseTrackerModuleMetadata.h"

@implementation SFBImpulseTrackerModuleMetadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"it"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/it"];
}

+ (BOOL)_handlesFilesWithExtension:(NSString *)extension
{
	return [extension caseInsensitiveCompare:@"it"] == NSOrderedSame;
}

+ (BOOL)_handlesMIMEType:(NSString *)mimeType
{
	return [mimeType caseInsensitiveCompare:@"audio/it"] == NSOrderedSame;
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

	TagLib::IT::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Impulse Tracker module file.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not an Impulse Tracker module file", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"Impulse Tracker Module";

	if(file.audioProperties())
		[self addAudioPropertiesFromTagLibAudioProperties:file.audioProperties()];

	if(file.tag())
		[self addMetadataFromTagLibTag:file.tag()];

	return YES;
}

- (BOOL)_writeMetadata:(NSError **)error
{
	os_log_error(OS_LOG_DEFAULT, "Writing Impulse Tracker module metadata is not supported");

	if(error)
		*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
						   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
													 url:self.url
										   failureReason:NSLocalizedString(@"Unable to write metadata", @"")
									  recoverySuggestion:NSLocalizedString(@"Writing Impulse Tracker module metadata is not supported.", @"")];
	return NO;
}

@end
