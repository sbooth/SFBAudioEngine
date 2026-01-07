//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBPCMDecoder.h"

#import "SFBTernaryTruthValue.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBPCMDecoderLog;

@interface SFBPCMDecoder ()
{
@package
	SFBInputSource *_inputSource;
	AVAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
	NSDictionary *_properties;
}
/// Returns the decoder name
@property (class, nonatomic, readonly) SFBPCMDecoderName decoderName;

/// Tests whether a seekable input source contains data in a supported format
/// - parameter inputSource: The input source containing the data to test
/// - parameter formatIsSupported: On return indicates whether the data in `inputSource` is a supported format
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the test was successfully performed, `NO` otherwise
+ (BOOL)testInputSource:(SFBInputSource *)inputSource formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error;
@end

#pragma mark - Subclass Registration

@interface SFBPCMDecoder (SFBPCMDecoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
