/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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
