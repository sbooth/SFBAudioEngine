//
// Copyright (c) 2010-2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBFileContentsInputSource.h"
#import "SFBInputSource+Internal.h"

@implementation SFBFileContentsInputSource

- (instancetype)initWithContentsOfURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	NSParameterAssert(url.isFileURL);

	NSData *data = [NSData dataWithContentsOfURL:url options:0 error:error];
	if(data == nil)
		return nil;
	
	if((self = [super initWithData:data]))
		_url = url;
	return self;
}

@end
