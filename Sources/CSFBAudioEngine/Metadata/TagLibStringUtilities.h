//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <taglib/tstring.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

namespace TagLib {

/// Creates a `TagLib::String` from the specified Foundation string
inline String StringFromNSString(NSString *_Nullable s) { return s ? String(s.UTF8String, String::UTF8) : String(); }

} /* namespace TagLib */

NS_ASSUME_NONNULL_END
