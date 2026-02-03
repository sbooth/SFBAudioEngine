//
// SPDX-FileCopyrightText: 2011 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#define SFBCStringForOSType(osType)                                                                                    \
    (const char[]){*(((char *)&osType) + 3), *(((char *)&osType) + 2), *(((char *)&osType) + 1),                       \
                   *(((char *)&osType) + 0), 0}
