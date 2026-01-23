//
// Copyright (c) 2010-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBFileContentsInputSource.h"

@implementation SFBFileContentsInputSource

- (instancetype)initWithContentsOfURL:(NSURL *)url error:(NSError **)error {
    NSParameterAssert(url != nil);
    NSParameterAssert(url.isFileURL);

    NSData *data = [NSData dataWithContentsOfURL:url options:0 error:error];
    if (data == nil) {
        return nil;
    }

    return [super initWithData:data url:url];
}

@end
