//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBLocalizedNameForURL.h"

NSString *SFBLocalizedNameForURL(NSURL *url) {
    if (!url) {
        return nil;
    }
    NSString *localizedName = nil;
    if (![url getResourceValue:&localizedName forKey:NSURLLocalizedNameKey error:nil] || localizedName == nil) {
        return url.lastPathComponent;
    }
    return localizedName;
}
