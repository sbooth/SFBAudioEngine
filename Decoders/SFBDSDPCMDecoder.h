//
// Copyright (c) 2018 - 2021 Stephen F. Booth <me@sbooth.org>
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

/// Returns an initialized \c SFBDSDPCMDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBDSDPCMDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized \c SFBDSDPCMDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBDSDPCMDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized \c SFBDSDPCMDecoder object for the given decoder or \c nil on failure
/// @param decoder The decoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBDSDPCMDecoder object for the specified decoder, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBDSDDecoding>)decoder error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// The linear gain applied to the converted DSD samples (default is 6 dBFS)
@property (nonatomic) float linearGain;

@end

NS_ASSUME_NONNULL_END
