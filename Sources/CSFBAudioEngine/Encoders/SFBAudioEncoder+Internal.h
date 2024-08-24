//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

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
/// Returns the encoder name
@property (class, nonatomic, readonly) SFBAudioEncoderName encoderName;
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBAudioEncoder (SFBAudioEncoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

@interface SFBAudioEncoder (SFBAudioEncoderSubclassLookup)
/// Returns the appropriate `SFBAudioEncoder` subclass for encoding `url`
+ (nullable Class)subclassForURL:(NSURL *)url;
/// Returns the appropriate `SFBAudioEncoder` subclass for encoding paths with `extension`
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
/// Returns the appropriate `SFBAudioEncoder` subclass for encoding data of `mimeType`
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
/// Returns the appropriate `SFBAudioEncoder` subclass corresponding to `encoderName`
+ (nullable Class)subclassForEncoderName:(SFBAudioEncoderName)encoderName;
@end

NS_ASSUME_NONNULL_END
