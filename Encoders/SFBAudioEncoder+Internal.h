/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioEncoder.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioEncoderLog;

@interface SFBAudioEncoder ()
{
@package
	SFBOutputSource *_outputSource;
@protected
	AVAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
	AVAudioFormat *_outputFormat;
	AVAudioFramePosition _estimatedFramesToEncode;
	NSDictionary *_settings;
}
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBAudioEncoder (SFBAudioEncoderSubclassRegistration)
/// Register a subclass with the default priority (\c 0)
+ (void)registerSubclass:(Class)subclass;

/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

@interface SFBAudioEncoder (SFBAudioEncoderSubclassLookup)
+ (nullable Class)subclassForURL:(NSURL *)url;
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
@end

NS_ASSUME_NONNULL_END
