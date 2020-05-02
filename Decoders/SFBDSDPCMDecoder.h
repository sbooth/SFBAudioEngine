/*
 * Copyright (c) 2018 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBPCMDecoding.h"
#import "SFBDSDDecoding.h"

NS_ASSUME_NONNULL_BEGIN

//! A wrapper around a DSD decoder supporting DSD64 to PCM conversion
NS_SWIFT_NAME(DSDPCMDecoder) @interface SFBDSDPCMDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
- (nullable instancetype)initWithDecoder:(id <SFBDSDDecoding>)decoder error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/*! @brief The linear gain applied to the converted DSD samples (default is 6 dBFS) */
@property (nonatomic) float linearGain;

@end

NS_ASSUME_NONNULL_END
