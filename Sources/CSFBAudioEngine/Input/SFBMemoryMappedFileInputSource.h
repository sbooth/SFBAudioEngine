//
// Copyright (c) 2010-2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBDataInputSource.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBMemoryMappedFileInputSource : SFBDataInputSource
- (instancetype)initWithData:(NSData *)data NS_UNAVAILABLE;
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
