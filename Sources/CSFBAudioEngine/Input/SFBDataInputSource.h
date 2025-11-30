//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
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
