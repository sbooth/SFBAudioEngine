/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import OSLog;

#import "SFBHTTPInputSource.h"
#import "SFBInputSource+Internal.h"

@interface SFBHTTPInputSource ()
{
@private
	NSURLSessionDataTask 	*_dataTask;
	NSUInteger				_expectedLength;
	NSMutableData 			*_data;
	NSUInteger				_pos;
	NSInteger 				_start;
}
@end

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

	if(_start > 0)
		[request setValue:[NSString stringWithFormat:@"bytes=%ld-", _start] forHTTPHeaderField:@"Range"];

	NSURLSession *session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration] delegate:self delegateQueue:nil];

	_dataTask = [session dataTaskWithRequest:request];
	[_dataTask resume];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	[_dataTask cancel];
	_dataTask = nil;
	_data = nil;
	return YES;
}

- (BOOL)isOpen
{
	return _data != nil;
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error
{
	NSParameterAssert(length > 0);

	NSUInteger count = (NSUInteger)length;
	NSUInteger remaining = _data.length - _pos;
	if(count > remaining)
		count = remaining;

	[_data getBytes:buffer range:NSMakeRange(_pos, count)];
	_pos += count;
	*bytesRead = (NSInteger)count;

	return YES;
}

- (BOOL)atEOF
{
	return _dataTask.state == NSURLSessionTaskStateCompleted && _pos == _data.length;
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error
{
	*offset = (NSInteger)_pos + _start;
	return YES;
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error
{
	*length = _dataTask.state == NSURLSessionTaskStateCompleted ? (NSInteger)_data.length : (NSInteger)_expectedLength;
	return YES;
}

- (BOOL)supportsSeeking
{
	return YES;
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error
{
	NSParameterAssert(offset >= 0);

	if(![self closeReturningError:error])
		return NO;

	_pos = 0;
	_expectedLength = 0;
	_start = offset;

	return [self openReturningError:error];
}

@end

@implementation SFBHTTPInputSource (NSURLSessionDataDelegate)

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler
{
	NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
	NSInteger statusCode = httpResponse.statusCode;
	os_log_debug(gSFBInputSourceLog, "HTTP status: %ld %{public}@", (long)statusCode, [NSHTTPURLResponse localizedStringForStatusCode:statusCode]);
	if(statusCode < 200 || statusCode > 299) {
		completionHandler(NSURLSessionResponseCancel);
		return;
	}

	if(httpResponse.expectedContentLength != NSURLResponseUnknownLength)
		_expectedLength = (NSUInteger)httpResponse.expectedContentLength;

	_data = [NSMutableData data];

	completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
	[_data appendData:data];
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
	if(error)
		os_log_error(gSFBInputSourceLog, "NSURLSessionData error: %{public}@", error);
}

@end
