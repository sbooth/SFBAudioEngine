/*
 * Copyright (c) 2012 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

/*! @file CFDictionaryUtilities.h @brief Utility methods for adding primitive types to \c CFDictionary */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief Add an \c int to the specified dictionary as an \c CFNumber */
	void AddIntToDictionary(CFMutableDictionaryRef d, CFStringRef key, int value);

	/*! @brief Add an \c int to the specified dictionary as an \c CFString */
	void AddIntToDictionaryAsString(CFMutableDictionaryRef d, CFStringRef key, int value);


	/*! @brief Add a \c long to the specified dictionary as an \c CFNumber */
	void AddLongLongToDictionary(CFMutableDictionaryRef d, CFStringRef key, long long value);


	/*! @brief Add a \c float to the specified dictionary as an \c CFNumber */
	void AddFloatToDictionary(CFMutableDictionaryRef d, CFStringRef key, float value);

	/*! @brief Add a \c double to the specified dictionary as an \c CFNumber */
	void AddDoubleToDictionary(CFMutableDictionaryRef d, CFStringRef key, double value);

}
