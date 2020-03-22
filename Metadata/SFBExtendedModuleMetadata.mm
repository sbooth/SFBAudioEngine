/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <os/log.h>

#import <taglib/tfilestream.h>
#import <taglib/xmfile.h>

#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBExtendedModuleMetadata.h"

@implementation SFBExtendedModuleMetadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"xm"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/xm"];
}

+ (BOOL)_handlesFilesWithExtension:(NSString *)extension
{
	return [extension caseInsensitiveCompare:@"xm"] == NSOrderedSame;
}

+ (BOOL)_handlesMIMEType:(NSString *)mimeType
{
	return [mimeType caseInsensitiveCompare:@"audio/xm"] == NSOrderedSame;
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

	TagLib::XM::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
							   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid extended module.", @"")
														 url:self.url
											   failureReason:NSLocalizedString(@"Not an extended module", @"")
										  recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"Extended Module";

	if(file.audioProperties())
		[self addAudioPropertiesFromTagLibAudioProperties:file.audioProperties()];

	if(file.tag())
		[self addMetadataFromTagLibTag:file.tag()];

	return YES;
}

- (BOOL)_writeMetadata:(NSError **)error
{
	os_log_error(OS_LOG_DEFAULT, "Writing extended module metadata is not supported");

	if(error)
		*error = [NSError sfb_audioMetadataErrorWithCode:SFBAudioMetadataErrorCodeInputOutput
						   descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
													 url:self.url
										   failureReason:NSLocalizedString(@"Unable to write metadata", @"")
									  recoverySuggestion:NSLocalizedString(@"Writing extended module metadata is not supported.", @"")];
	return NO;
}

@end
