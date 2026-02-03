//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBOutputSource+Internal.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBBufferOutputSource : SFBOutputSource
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithURL:(nullable NSURL *)url NS_UNAVAILABLE;
- (instancetype)initWithBuffer:(void *)buffer capacity:(size_t)capacity NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
