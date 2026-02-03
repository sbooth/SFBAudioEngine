//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
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
