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
	NSDictionary *_settings;
}
@end

@interface SFBAudioEncoderSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@interface SFBAudioEncoder (SFBAudioEncoderSubclassLookup)
+ (nullable Class)subclassForURL:(NSURL *)url;
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
@end

NS_ASSUME_NONNULL_END
