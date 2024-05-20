//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBOutputSource.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBBufferOutputSource : SFBOutputSource
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBuffer:(void *)buffer capacity:(size_t)capacity NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
