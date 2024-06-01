//
// Copyright (c) 2010-2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBInputSource.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBDataInputSource : SFBInputSource
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithData:(NSData *)data NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
