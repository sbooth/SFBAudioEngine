//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "InputSource.hpp"
#import "SFBInputSource.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBInputSource () {
  @package
    SFB::InputSource::unique_ptr _input;
}
@end

NS_ASSUME_NONNULL_END
