//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBInputSource.h"

#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBInputSourceLog;

@interface SFBInputSource () {
  @package
    NSURL *_url;
}
- (instancetype)initWithURL:(nullable NSURL *)url NS_DESIGNATED_INITIALIZER;
- (NSError *)posixErrorWithCode:(NSInteger)code;
@end

NS_ASSUME_NONNULL_END
