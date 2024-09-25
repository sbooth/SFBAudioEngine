//
// Copyright (c) 2010-2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBInputSource.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBInputSourceLog;

@interface SFBInputSource ()
{
@protected
	NSURL *_url;
}
@end

NS_ASSUME_NONNULL_END
