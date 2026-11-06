//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBFileContentsInputSource.h"

#import <assert.h>

@implementation SFBFileContentsInputSource

- (instancetype)initWithContentsOfURL:(NSURL *)url error:(NSError **)error {
    assert(url != nil);
    assert(url.isFileURL);

    NSData *data = [NSData dataWithContentsOfURL:url options:0 error:error];
    if (!data) {
        return nil;
    }

    return [super initWithData:data url:url];
}

@end
