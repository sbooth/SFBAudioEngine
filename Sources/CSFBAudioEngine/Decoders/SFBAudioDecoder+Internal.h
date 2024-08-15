//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBAudioDecoder.h"

#import "SFBTernaryTruthValue.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioDecoderLog;

@interface SFBAudioDecoder ()
{
@package
	SFBInputSource *_inputSource;
@protected
	AVAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
	NSDictionary *_properties;
}
/// Returns the decoder name
@property (class, nonatomic, readonly) SFBAudioDecoderName decoderName;

/// Tests whether a seekable input source contains data in a supported format
/// - parameter inputSource: The input source containing the data to test
/// - parameter formatIsSupported: On return indicates whether the data in `inputSource` is a supported format
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the test was successfully performed, `NO` otherwise
+ (BOOL)testInputSource:(SFBInputSource *)inputSource formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error;
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
