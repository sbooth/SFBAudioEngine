
/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBInputSource.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBDataInputSource : SFBInputSource
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithData:(NSData *)data NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithBytes:(const void *)bytes length:(NSInteger)length;
- (instancetype)initWithBytesNoCopy:(void *)bytes length:(NSInteger)length;
@end

NS_ASSUME_NONNULL_END
