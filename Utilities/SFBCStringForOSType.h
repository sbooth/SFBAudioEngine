/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

/*! @file SFBCStringForOSType.h @brief OSType to const char [] conversion */

#define SFBCStringForOSType(osType) (const char[]){ *(((char *)&osType) + 3), *(((char *)&osType) + 2), *(((char *)&osType) + 1), *(((char *)&osType) + 0), 0 }
