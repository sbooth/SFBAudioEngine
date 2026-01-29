//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBOutputSource.h"

#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBOutputSourceLog;

@interface SFBOutputSource () {
  @package
    NSURL *_url;
}
- (instancetype)initWithURL:(nullable NSURL *)url NS_DESIGNATED_INITIALIZER;
- (NSError *)posixErrorWithCode:(NSInteger)code;
@end

NS_ASSUME_NONNULL_END
