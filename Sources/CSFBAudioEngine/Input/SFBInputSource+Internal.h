//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
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
