
/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBDataInputSource.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBMemoryMappedFileInputSource : SFBDataInputSource
- (instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError * _Nullable *)error NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
