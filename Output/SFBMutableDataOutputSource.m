/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBMutableDataOutputSource.h"

@interface SFBMutableDataOutputSource ()
{
@private
	NSMutableData	*_data;
	NSUInteger		_pos;
}
@end

@implementation SFBMutableDataOutputSource

- (instancetype)initWithMutableData:(NSMutableData *)data
{
	NSParameterAssert(data != nil);

	if((self = [super init]))
		_data = data;
	return self;
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

- (BOOL)writeBytes:(const void *)buffer length:(NSInteger)length bytesWritten:(NSInteger *)bytesWritten error:(NSError **)error
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(length > 0);
	NSParameterAssert(bytesWritten != NULL);

	[_data appendBytes:buffer length:(NSUInteger)length];
	_pos += (NSUInteger)length;
	*bytesWritten = length;
	return YES;
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

	if((NSUInteger)offset >= _data.length)
		[_data increaseLengthBy:((NSUInteger)offset - _data.length + 1)];

	_pos = (NSUInteger)offset;
	return YES;
}

@end
