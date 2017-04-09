/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

/*! @file CreateDisplayNameForURL.h @brief URL display name creation */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief Get the localized display name for a URL */
	CFStringRef CreateDisplayNameForURL(CFURLRef url);

}
