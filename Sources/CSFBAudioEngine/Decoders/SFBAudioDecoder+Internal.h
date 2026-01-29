//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioDecoder.h"
#import "SFBTernaryTruthValue.h"

#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioDecoderLog;

@interface SFBAudioDecoder () {
  @package
    SFBInputSource *_inputSource;
    AVAudioFormat *_sourceFormat;
    AVAudioFormat *_processingFormat;
    NSDictionary *_properties;
}
/// Returns the decoder name
@property(class, nonatomic, readonly) SFBAudioDecoderName decoderName;

/// Tests whether a seekable input source contains data in a supported format
/// - parameter inputSource: The input source containing the data to test
/// - parameter formatIsSupported: On return indicates whether the data in `inputSource` is a supported format
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the test was successfully performed, `NO` otherwise
+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error;

/// Returns an invalid format error with a description similar to "The file is not a valid XXX file"
/// - parameter formatName: The localized name of the audio format
/// - returns: An error in `SFBAudioDecoderErrorDomain` with code `SFBAudioDecoderErrorCodeInvalidFormat`
- (NSError *)invalidFormatError:(NSString *)formatName;
/// Returns an invalid format error with a description similar to "The file is not a valid XXX file"
/// - parameter formatName: The localized name of the audio format
/// - parameter recoverySuggestion: A localized error recovery suggestion
/// - returns: An error in `SFBAudioDecoderErrorDomain` with code `SFBAudioDecoderErrorCodeInvalidFormat`
- (NSError *)invalidFormatError:(NSString *)formatName recoverySuggestion:(NSString *)recoverySuggestion;
/// Returns an unsupported format error with a description similar to "The file is not a supported XXX file"
/// - parameter formatName: The localized name of the audio format
/// - parameter recoverySuggestion: A localized error recovery suggestion
/// - returns: An error in `SFBAudioDecoderErrorDomain` with code `SFBAudioDecoderErrorCodeUnsupportedFormat`
- (NSError *)unsupportedFormatError:(NSString *)formatName recoverySuggestion:(NSString *)recoverySuggestion;
/// Returns a generic internal error
/// - returns: An error in `SFBAudioDecoderErrorDomain` with code `SFBAudioDecoderErrorCodeInternalError`
- (NSError *)genericInternalError;
/// Returns a generic decoding error
/// - returns: An error in `SFBAudioDecoderErrorDomain` with code `SFBAudioDecoderErrorCodeDecodingError`
- (NSError *)genericDecodingError;
/// Returns a generic seek error
/// - returns: An error in `SFBAudioDecoderErrorDomain` with code `SFBAudioDecoderErrorCodeSeekError`
- (NSError *)genericSeekError;
@end

#pragma mark - Subclass Registration

@interface SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
