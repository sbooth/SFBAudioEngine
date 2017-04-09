/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include "CFWrapper.h"

/*! @file CreateStringForOSType.h @brief OSType to CFString conversion */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief Create a string representation of the four character code \c osType */
	CFString StringForOSType(OSType osType);

}
