/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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

	NSData *data = [NSData dataWithContentsOfFile:url.path options:NSDataReadingMappedAlways error:error];
	if(data == nil)
		return nil;

	if((self = [super init]))
		_url = url;
	return self;
}

- (BOOL)openReturningError:(NSError **)error
{
	_file = fopen(self.url.fileSystemRepresentation, "r");
	if(!_file) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
		return NO;
	}

	if(fstat(fileno(_file), &_filestats) == -1) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_file) {
		if(fclose(_file)) {
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
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

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error
{
	size_t read = fread(buffer, 1, (size_t)length, _file);
	if(read != (size_t)length && ferror(_file)) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
		return NO;
	}
	*bytesRead = (NSInteger)read;
	return YES;
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error
{
	off_t result = ftello(_file);
	if(result == -1) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
		return NO;
	}
	*offset = result;
	return YES;
}

- (BOOL)atEOF
{
	return feof(_file) != 0;
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error
{
	*length = _filestats.st_size;
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
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
		return NO;
	}
	return YES;
}

@end
