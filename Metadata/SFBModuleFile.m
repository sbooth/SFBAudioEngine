/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#include <dumb/dumb.h>

#import "SFBModuleFile.h"

#import "NSError+SFBURLPresentation.h"

#define DUMB_SAMPLE_RATE	65536
#define DUMB_CHANNELS		2
#define DUMB_BIT_DEPTH		16

@implementation SFBModuleFile

+ (void)load
{
	dumb_register_stdfiles();
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"it", @"xm", @"s3m", @"mod"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/it", @"audio/xm", @"audio/s3m", @"audio/mod", @"audio/x-mod"]];
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	DUMBFILE *df = dumbfile_open(self.url.fileSystemRepresentation);
	if(!df) {
		os_log_error(gSFBAudioFileLog, "dumbfile_open failed");
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EIO userInfo:nil];
		return NO;
	}

	DUH *duh = NULL;
	NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionary];
	NSString *pathExtension = self.url.pathExtension.lowercaseString;

	// Attempt to create the appropriate decoder based on the file's extension
	if([pathExtension isEqualToString:@"it"]) {
		duh = dumb_read_it_quick(df);
		propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"Impulse Tracker Module";
	}
	else if([pathExtension isEqualToString:@"xm"]) {
		duh = dumb_read_xm_quick(df);
		propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"Extended Module";
	}
	else if([pathExtension isEqualToString:@"s3m"]) {
		duh = dumb_read_s3m_quick(df);
		propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"Scream Tracker 3 Module";
	}
	else if([pathExtension isEqualToString:@"mod"]) {
		duh = dumb_read_mod_quick(df);
		propertiesDictionary[SFBAudioPropertiesKeyFormatName] = @"ProTracker Module";
	}

	if(!duh) {
		dumbfile_close(df);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Module file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not a Module file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(duh_get_length(duh));
	propertiesDictionary[SFBAudioPropertiesKeySampleRate] = @(DUMB_SAMPLE_RATE);
	propertiesDictionary[SFBAudioPropertiesKeyChannelCount] = @(DUMB_CHANNELS);
	propertiesDictionary[SFBAudioPropertiesKeyBitsPerChannel] = @(DUMB_BIT_DEPTH);
	propertiesDictionary[SFBAudioPropertiesKeyDuration] = @(duh_get_length(duh) / (float)DUMB_SAMPLE_RATE);

	self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
	self.metadata = [[SFBAudioMetadata alloc] initWithDictionaryRepresentation:@{ SFBAudioMetadataKeyTitle: @(duh_get_tag(duh, "TITLE")) }];;

	unload_duh(duh);
	dumbfile_close(df);

	return YES;
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	os_log_error(gSFBAudioFileLog, "Writing Module metadata is not supported");

	if(error)
		*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
										 code:SFBAudioFileErrorCodeInputOutput
				descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
										  url:self.url
								failureReason:NSLocalizedString(@"Unable to write metadata", @"")
						   recoverySuggestion:NSLocalizedString(@"Writing Module metadata is not supported.", @"")];
	return NO;
}

@end
