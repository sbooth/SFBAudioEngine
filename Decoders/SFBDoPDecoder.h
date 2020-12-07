/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBDSDDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// A wrapper around a DSD decoder supporting DoP (DSD over PCM)
/// @see http://dsd-guide.com/sites/default/files/white-papers/DoP_openStandard_1v1.pdf
NS_SWIFT_NAME(DoPDecoder) @interface SFBDoPDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBDoPDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBDoPDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized \c SFBDoPDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBDoPDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized \c SFBDoPDecoder object for the given decoder or \c nil on failure
/// @param decoder The decoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBDoPDecoder object for the specified decoder, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBDSDDecoding>)decoder error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
