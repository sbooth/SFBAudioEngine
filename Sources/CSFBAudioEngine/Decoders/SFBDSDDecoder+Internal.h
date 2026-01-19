//
// Copyright (c) 2014-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBDSDDecoder.h"
#import "SFBTernaryTruthValue.h"

#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBDSDDecoderLog;

@interface SFBDSDDecoder () {
  @package
    SFBInputSource *_inputSource;
    AVAudioFormat *_sourceFormat;
    AVAudioFormat *_processingFormat;
    NSDictionary *_properties;
}
/// Returns the decoder name
@property(class, nonatomic, readonly) SFBDSDDecoderName decoderName;

/// Tests whether a seekable input source contains data in a supported format
/// - parameter inputSource: The input source containing the data to test
/// - parameter formatIsSupported: On return indicates whether the data in `inputSource` is a supported format
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the test was successfully performed, `NO` otherwise
+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error;
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBDSDDecoder (SFBDSDDecoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
