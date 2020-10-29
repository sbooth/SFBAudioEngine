
/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBOutputSource.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBMutableDataOutputSource : SFBOutputSource
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMutableData:(NSMutableData *)data NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
