/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBDataInputSource.h"

@interface SFBDataInputSource ()
{
@private
	NSData 		*_data;
	NSUInteger	_pos;
}
@end

@implementation SFBDataInputSource

- (instancetype)initWithData:(NSData *)data
{
	NSParameterAssert(data != nil);

	if((self = [super init]))
		_data = [data copy];
	return self;
}

- (instancetype)initWithBytes:(const void *)bytes length:(NSInteger)length
{
	NSParameterAssert(bytes != NULL);
	NSParameterAssert(length >= 0);

	NSData *data = [NSData dataWithBytes:bytes length:(NSUInteger)length];
	if(data == nil)
		return nil;
	return [self initWithData:data];
}

- (instancetype)initWithBytesNoCopy:(void *)bytes length:(NSInteger)length freeWhenDone:(BOOL)freeWhenDone
{
	NSParameterAssert(bytes != NULL);
	NSParameterAssert(length >= 0);

	NSData *data = [NSData dataWithBytesNoCopy:bytes length:(NSUInteger)length freeWhenDone:freeWhenDone];
	if(data == nil)
		return nil;
	return [self initWithData:data];
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
	if((NSUInteger)offset >= _data.length)
		return NO;

	_pos = (NSUInteger)offset;
	return YES;
}

@end
