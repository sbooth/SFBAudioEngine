//
// Copyright (c) 2014-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBDSDDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// A wrapper around a DSD decoder supporting DoP (DSD over PCM)
/// - seealso: http://dsd-guide.com/sites/default/files/white-papers/DoP_openStandard_1v1.pdf
NS_SWIFT_NAME(DoPDecoder) @interface SFBDoPDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBDoPDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDoPDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBDoPDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDoPDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized `SFBDoPDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDoPDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id <SFBDSDDecoding>)decoder error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
