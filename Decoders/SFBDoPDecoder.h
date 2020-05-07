/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBPCMDecoding.h"
#import "SFBDSDDecoding.h"

NS_ASSUME_NONNULL_BEGIN

/// A wrapper around a DSD decoder supporting DoP (DSD over PCM)
/// @see http://dsd-guide.com/sites/default/files/white-papers/DoP_openStandard_1v1.pdf
NS_SWIFT_NAME(DoPDecoder) @interface SFBDoPDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
- (nullable instancetype)initWithDecoder:(id <SFBDSDDecoding>)decoder error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
