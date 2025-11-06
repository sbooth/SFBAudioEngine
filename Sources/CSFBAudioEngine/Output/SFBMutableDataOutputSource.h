//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBOutputSource+Internal.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBMutableDataOutputSource : SFBOutputSource
- (instancetype)initWithURL:(nullable NSURL *)url NS_UNAVAILABLE;
@end

NS_ASSUME_NONNULL_END
