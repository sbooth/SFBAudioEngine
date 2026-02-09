//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBAudioEncoder.h"

#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioEncoderLog;

@interface SFBAudioEncoder () {
  @package
    SFBOutputTarget *_outputTarget;
    AVAudioFormat *_sourceFormat;
    AVAudioFormat *_processingFormat;
    AVAudioFormat *_outputFormat;
    AVAudioFramePosition _estimatedFramesToEncode;
    NSDictionary *_settings;
}
/// Returns the encoder name
@property(class, nonatomic, readonly) SFBAudioEncoderName encoderName;
@end

// MARK: - Subclass Registration

@interface SFBAudioEncoder (SFBAudioEncoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
