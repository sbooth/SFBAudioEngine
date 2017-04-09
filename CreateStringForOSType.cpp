/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "CreateStringForOSType.h"

SFB::CFString SFB::StringForOSType(OSType osType)
{
	unsigned char formatID [4];
	*(UInt32 *)formatID = OSSwapHostToBigInt32(osType);

    return CFString(nullptr, CFSTR("%.4s"), formatID);
}
