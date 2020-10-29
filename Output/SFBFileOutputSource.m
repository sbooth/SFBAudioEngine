/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <stdio.h>

#import "SFBFileOutputSource.h"
#import "SFBOutputSource+Internal.h"

@interface SFBFileOutputSource ()
{
@private
	FILE *_file;
}
@end

@implementation SFBFileOutputSource

- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	NSParameterAssert(url.isFileURL);

	if((self = [super init]))
		_url = url;
	return self;
}

- (BOOL)openReturningError:(NSError **)error
{
	_file = fopen(self.url.fileSystemRepresentation, "w");
	if(!_file) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_file) {
		if(fclose(_file)) {
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
			return NO;
		}
		_file = NULL;
	}
	return YES;
}

- (BOOL)isOpen
{
	return _file != NULL;
}

- (BOOL)writeBytes:(const void *)buffer length:(NSInteger)length bytesWritten:(NSInteger *)bytesWritten error:(NSError **)error
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(length > 0);
	NSParameterAssert(bytesWritten != NULL);

	size_t written = fwrite(buffer, 1, (size_t)length, _file);
	if(written != (size_t)length) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}
	*bytesWritten = (NSInteger)written;
	return YES;
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error
{
	NSParameterAssert(offset != NULL);
	off_t result = ftello(_file);
	if(result == -1) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}
	*offset = result;
	return YES;
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error
{
	NSParameterAssert(length != NULL);
	off_t offset = ftello(_file);
	if(offset == -1) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}

	if(fseeko(_file, 0, SEEK_END)) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}

	off_t len = ftello(_file);
	if(len == -1) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}

	if(fseeko(_file, offset, SEEK_SET)) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}

	*length = len;

	return YES;
}

- (BOOL)supportsSeeking
{
	return YES;
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error
{
	if(fseeko(_file, offset, SEEK_SET)) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}
	return YES;
}

@end
