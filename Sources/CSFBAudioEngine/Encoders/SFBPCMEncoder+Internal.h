//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBPCMEncoder.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBPCMEncoderLog;

@interface SFBPCMEncoder ()
{
@package
	SFBOutputSource *_outputSource;
	AVAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
	AVAudioFormat *_outputFormat;
	AVAudioFramePosition _estimatedFramesToEncode;
	NSDictionary *_settings;
}
/// Returns the encoder name
@property (class, nonatomic, readonly) SFBPCMEncoderName encoderName;
@end

#pragma mark - Subclass Registration

@interface SFBPCMEncoder (SFBPCMEncoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
