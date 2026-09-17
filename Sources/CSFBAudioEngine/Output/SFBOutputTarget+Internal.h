//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <os/log.h>

#import "SFBOutputTarget.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBOutputTargetLog;

@interface SFBOutputTarget () {
  @package
    NSURL *_url;
}
- (instancetype)initWithURL:(nullable NSURL *)url NS_DESIGNATED_INITIALIZER;
- (NSError *)posixErrorWithCode:(NSInteger)code;
@end

NS_ASSUME_NONNULL_END
