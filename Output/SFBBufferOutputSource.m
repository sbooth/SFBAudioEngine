/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBBufferOutputSource.h"

@interface SFBBufferOutputSource ()
{
@private
	void		*_buffer;
	size_t 		_capacity;
	size_t		_pos;
}
@end

@implementation SFBBufferOutputSource

- (instancetype)initWithBuffer:(void *)buffer capacity:(size_t)capacity
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert(capacity > 0);

	if((self = [super init])) {
		_buffer = buffer;
		_capacity = capacity;
	}
	return self;
}

- (BOOL)openReturningError:(NSError **)error
{
	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_buffer = nil;
	return YES;
}

- (BOOL)isOpen
{
	return _buffer != nil;
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(length >= 0);
	NSParameterAssert(bytesRead != NULL);

	size_t bytesAvailable = _capacity - _pos;
	if(bytesAvailable == 0) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
		return NO;
	}

	size_t bytesToCopy = MIN(bytesAvailable, (size_t)length);
	memcpy(buffer, (uint8_t *)_buffer + _pos, bytesToCopy);
	_pos += bytesToCopy;
	*bytesRead = (NSInteger)bytesToCopy;

	return YES;
}

- (BOOL)writeBytes:(const void *)buffer length:(NSInteger)length bytesWritten:(NSInteger *)bytesWritten error:(NSError **)error
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(length > 0);
	NSParameterAssert(bytesWritten != NULL);

	size_t remainingCapacity = _capacity - _pos;
	if(remainingCapacity == 0) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
		return NO;
	}

	size_t bytesToCopy = MIN(remainingCapacity, (size_t)length);
	memcpy((uint8_t *)_buffer + _pos, buffer, bytesToCopy);
	_pos += bytesToCopy;
	*bytesWritten = (NSInteger)bytesToCopy;

	return YES;
}

- (BOOL)atEOF
{
	return _pos == _capacity;
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error
{
	NSParameterAssert(offset != NULL);
	*offset = (NSInteger)_pos;
	return YES;
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error
{
	NSParameterAssert(length != NULL);
	*length = (NSInteger)_capacity;
	return YES;
}

- (BOOL)supportsSeeking
{
	return YES;
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error
{
	NSParameterAssert(offset >= 0);

	if((NSUInteger)offset >= _capacity) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
		return NO;
	}

	_pos = (NSUInteger)offset;
	return YES;
}

@end
