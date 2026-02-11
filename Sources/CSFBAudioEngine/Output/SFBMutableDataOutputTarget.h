//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBOutputTarget+Internal.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBMutableDataOutputTarget : SFBOutputTarget
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithURL:(nullable NSURL *)url NS_UNAVAILABLE;
@end

NS_ASSUME_NONNULL_END
