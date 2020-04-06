/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBHTTPInputSource.h"
#import "SFBInputSource+Internal.h"

@implementation SFBHTTPInputSource

- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	NSParameterAssert(!url.isFileURL);

	if((self = [super init]))
		_url = url;
	return self;
}

- (BOOL)openReturningError:(NSError **)error
{
	NSMutableURLRequest *request = [[NSMutableURLRequest alloc] init];
	request.URL = _url;
	request.HTTPMethod = @"GET";

	NSString *bundleName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
	NSString *bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
	[request setValue:[NSString stringWithFormat:@"%@ %@", bundleName, bundleVersion] forHTTPHeaderField:@"User-Agent"];

	NSURLSessionConfiguration *sessionConfig = [NSURLSessionConfiguration defaultSessionConfiguration];
	NSURLSession *session = [NSURLSession sessionWithConfiguration:sessionConfig];


//	// Seek support
//	if(0 < mDesiredOffset) {
//		SFB::CFString byteRange(nullptr, CFSTR("bytes=%lld-"), mDesiredOffset);
//		CFHTTPMessageSetHeaderFieldValue(mRequest, CFSTR("Range"), byteRange);
//	}


	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	return YES;
}

@end
