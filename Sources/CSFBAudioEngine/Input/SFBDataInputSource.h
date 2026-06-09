//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBInputSource+Internal.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBDataInputSource : SFBInputSource
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithURL:(nullable NSURL *)url NS_UNAVAILABLE;
- (instancetype)initWithData:(NSData *)data;
- (instancetype)initWithData:(NSData *)data url:(nullable NSURL *)url NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
