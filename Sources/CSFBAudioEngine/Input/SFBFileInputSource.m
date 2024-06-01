//
// Copyright (c) 2010-2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <stdio.h>
#import <sys/stat.h>

#import "SFBFileInputSource.h"
#import "SFBInputSource+Internal.h"

@interface SFBFileInputSource ()
{
@private
	struct stat		_filestats;
	FILE 			*_file;
}
@end

@implementation SFBFileInputSource

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
	_file = fopen(self.url.fileSystemRepresentation, "r");
	if(!_file) {
		os_log_error(gSFBInputSourceLog, "fopen failed: %{public}s (%d)", strerror(errno), errno);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}

	if(fstat(fileno(_file), &_filestats) == -1) {
		os_log_error(gSFBInputSourceLog, "fstat failed: %{public}s (%d)", strerror(errno), errno);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];

		if(fclose(_file))
			os_log_info(gSFBInputSourceLog, "fclose failed: %{public}s (%d)", strerror(errno), errno);
		_file = NULL;

		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_file) {
		int result = fclose(_file);
		_file = NULL;
		if(result) {
			os_log_error(gSFBInputSourceLog, "fclose failed: %{public}s (%d)", strerror(errno), errno);
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
			return NO;
		}
	}
	return YES;
}

- (BOOL)isOpen
{
	return _file != NULL;
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(length >= 0);
	NSParameterAssert(bytesRead != NULL);

	size_t read = fread(buffer, 1, (size_t)length, _file);
	if(read != (size_t)length && ferror(_file)) {
		os_log_error(gSFBInputSourceLog, "fread error: %{public}s (%d)", strerror(errno), errno);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}
	*bytesRead = (NSInteger)read;
	return YES;
}

- (BOOL)atEOF
{
	return feof(_file) != 0;
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error
{
	NSParameterAssert(offset != NULL);
	off_t result = ftello(_file);
	if(result == -1) {
		os_log_error(gSFBInputSourceLog, "ftello failed: %{public}s (%d)", strerror(errno), errno);
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
	*length = _filestats.st_size;
	return YES;
}

- (BOOL)supportsSeeking
{
	return YES;
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error
{
	NSParameterAssert(offset >= 0);
	if(fseeko(_file, offset, SEEK_SET)) {
		os_log_error(gSFBInputSourceLog, "fseeko(%ld, SEEK_SET) error: %{public}s (%d)", (long)offset, strerror(errno), errno);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSURLErrorKey: self.url }];
		return NO;
	}
	return YES;
}

@end
