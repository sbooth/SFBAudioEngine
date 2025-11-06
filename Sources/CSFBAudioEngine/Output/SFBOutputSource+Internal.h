//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBOutputSource.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBOutputSourceLog;

@interface SFBOutputSource ()
{
@protected
	NSURL *_url;
}
- (instancetype)initWithURL:(nullable NSURL *)url NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
