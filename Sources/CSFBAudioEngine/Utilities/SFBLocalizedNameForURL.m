//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBLocalizedNameForURL.h"

NSString *SFBLocalizedNameForURL(NSURL *url) {
    if (!url)
        return nil;
    NSString *localizedName = nil;
    if (![url getResourceValue:&localizedName forKey:NSURLLocalizedNameKey error:nil])
        return url.lastPathComponent;
    return localizedName ?: url.lastPathComponent;
}
