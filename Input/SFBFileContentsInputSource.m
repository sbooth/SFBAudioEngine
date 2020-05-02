/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBFileContentsInputSource.h"
#import "SFBInputSource+Internal.h"

@implementation SFBFileContentsInputSource

- (instancetype)initWithContentsOfURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	NSParameterAssert(url.isFileURL);

	NSData *data = [NSData dataWithContentsOfFile:url.path options:0 error:error];
	if(data == nil)
		return nil;
	
	if((self = [super initWithData:data]))
		_url = url;
	return self;
}

@end
