//
// Copyright (c) 2018-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBDSDDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// A wrapper around a DSD decoder supporting DSD64 to PCM conversion
NS_SWIFT_NAME(DSDPCMDecoder) @interface SFBDSDPCMDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBDSDPCMDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDPCMDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBDSDPCMDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDPCMDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized `SFBDSDPCMDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDPCMDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBDSDDecoding>)decoder error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// The linear gain applied to the converted DSD samples (default is 6 dBFS)
@property (nonatomic) float linearGain;

/// The underlying decoder
/// - warning: Do not change any properties of the returned object
@property (nonatomic, readonly) id<SFBDSDDecoding> decoder;

@end

NS_ASSUME_NONNULL_END
