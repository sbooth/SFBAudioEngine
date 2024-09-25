//
// Copyright (c) 2010-2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBMutableDataOutputSource.h"

@interface SFBMutableDataOutputSource ()
{
@private
	NSMutableData	*_data;
	NSUInteger		_pos;
}
@end

@implementation SFBMutableDataOutputSource

- (instancetype)init
{
	if((self = [super init]))
		_data = [NSMutableData data];
	return self;
}

- (NSData *)data
{
	return _data;
}

- (BOOL)openReturningError:(NSError **)error
{
	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_data = nil;
	return YES;
}

- (BOOL)isOpen
{
	return _data != nil;
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(length >= 0);
	NSParameterAssert(bytesRead != NULL);

	NSUInteger count = (NSUInteger)length;
	NSUInteger remaining = _data.length - _pos;
	if(count > remaining)
		count = remaining;

	[_data getBytes:buffer range:NSMakeRange(_pos, count)];
	_pos += count;
	*bytesRead = (NSInteger)count;

	return YES;
}

- (BOOL)writeBytes:(const void *)buffer length:(NSInteger)length bytesWritten:(NSInteger *)bytesWritten error:(NSError **)error
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(length >= 0);
	NSParameterAssert(bytesWritten != NULL);

	[_data appendBytes:buffer length:(NSUInteger)length];
	_pos += (NSUInteger)length;
	*bytesWritten = length;
	return YES;
}

- (BOOL)atEOF
{
	return _pos == _data.length;
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
	*length = (NSInteger)_data.length;
	return YES;
}

- (BOOL)supportsSeeking
{
	return YES;
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error
{
	NSParameterAssert(offset >= 0);

	if((NSUInteger)offset > _data.length)
		[_data increaseLengthBy:((NSUInteger)offset - _data.length + 1)];

	_pos = (NSUInteger)offset;
	return YES;
}

@end
