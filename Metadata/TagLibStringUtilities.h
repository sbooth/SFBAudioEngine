//
// Copyright (c) 2010 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#include <Foundation/Foundation.h>

#include <taglib/tstring.h>

/*! @file TagLibStringUtilities.h @brief Utilities for \c Taglib and Foundation interoperability */

NS_ASSUME_NONNULL_BEGIN

/*! @brief \c Taglib's encompassing namespace */
namespace TagLib {

	/*! @brief Create a \c TagLib::String from the specified Foundation string */
	inline String StringFromNSString(NSString * _Nullable s) { if(s) return {s.UTF8String, String::UTF8}; else return {}; }

}

NS_ASSUME_NONNULL_END
