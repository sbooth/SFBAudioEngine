//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBInputSource.h"
#import "InputSource.hpp"

NS_ASSUME_NONNULL_BEGIN

@interface SFBInputSource ()
{
@package
	SFB::InputSource::unique_ptr _input;
}
@end

NS_ASSUME_NONNULL_END
