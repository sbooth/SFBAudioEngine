/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <taglib/tstring.h>

/*! @file TagLibStringUtilities.h @brief Utilities for \c Taglib and Core Foundation interoperability */

/*! @brief \c Taglib's encompassing namespace */
namespace TagLib {

	/*! @brief Create a \c TagLib::String from the specified Core Foundation string */
	String StringFromCFString(CFStringRef s);

	/*!
	 * @brief Add a key/value pair to the specified dictionary
	 * @note This method does nothing if \c value is \c TagLib::String::null
	 * @param d The dictionary
	 * @param key The key
	 * @param value The value
	 */
	void AddStringToCFDictionary(CFMutableDictionaryRef d, CFStringRef key, String value);

}
