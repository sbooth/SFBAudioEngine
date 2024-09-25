//
// Copyright (c) 2011-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#define SFBCStringForOSType(osType) (const char[]){ *(((char *)&osType) + 3), *(((char *)&osType) + 2), *(((char *)&osType) + 1), *(((char *)&osType) + 0), 0 }
