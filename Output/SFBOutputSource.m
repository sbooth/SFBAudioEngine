/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBOutputSource.h"
#import "SFBOutputSource+Internal.h"

#import "SFBBufferOutputSource.h"
#import "SFBFileOutputSource.h"
#import "SFBMutableDataOutputSource.h"

os_log_t gSFBOutputSourceLog = NULL;

static void SFBCreateOutputSourceLog(void) __attribute__ ((constructor));
static void SFBCreateOutputSourceLog()
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBOutputSourceLog = os_log_create("org.sbooth.AudioEngine", "OutputSource");
	});
}

@implementation SFBOutputSource

+ (instancetype)outputSourceForURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	if(url.isFileURL)
		return [[SFBFileOutputSource alloc] initWithURL:url error:error];
	return nil;
}

+ (instancetype)dataOutputSource
{
	return [[SFBMutableDataOutputSource alloc] init];
}

+ (instancetype)outputSourceWithBuffer:(void *)buffer capacity:(NSInteger)capacity
{
	NSParameterAssert(buffer != NULL);
	NSParameterAssert(capacity >= 0);
	return [[SFBBufferOutputSource alloc] initWithBuffer:buffer capacity:(size_t)capacity];
}

- (void)dealloc
{
	if(self.isOpen)
		[self closeReturningError:nil];
}

- (NSData *)data
{
	return nil;
}

- (BOOL)openReturningError:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)closeReturningError:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)isOpen
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)writeBytes:(const void *)buffer length:(NSInteger)length bytesWritten:(NSInteger *)bytesWritten error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)supportsSeeking
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

@end

@implementation SFBOutputSource (SFBDataWriting)
- (BOOL)writeData:(NSData *)data error:(NSError **)error
{
	NSParameterAssert(data != nil);
	NSInteger bytesWritten;
	return [self writeBytes:data.bytes length:(NSInteger)data.length bytesWritten:&bytesWritten error:error] && bytesWritten == (NSInteger)data.length;
}
@end

@implementation SFBOutputSource (SFBSignedIntegerWriting)
- (BOOL)writeInt8:(int8_t)i8 error:(NSError **)error		{ return [self writeUInt8:(uint8_t)i8 error:error]; }
- (BOOL)writeInt16:(int16_t)i16 error:(NSError **)error		{ return [self writeUInt16:(uint16_t)i16 error:error]; }
- (BOOL)writeInt32:(int32_t)i32 error:(NSError **)error		{ return [self writeUInt32:(uint32_t)i32 error:error]; }
- (BOOL)writeInt64:(int64_t)i64 error:(NSError **)error		{ return [self writeUInt64:(uint64_t)i64 error:error]; }
@end

@implementation SFBOutputSource (SFBUnsignedIntegerWriting)
- (BOOL)writeUInt8:(uint8_t)ui8 error:(NSError **)error
{
	NSInteger bytesWritten;
	return [self writeBytes:&ui8 length:sizeof(uint8_t) bytesWritten:&bytesWritten error:error] && bytesWritten == sizeof(uint8_t);
}

- (BOOL)writeUInt16:(uint16_t)ui16 error:(NSError **)error
{
	NSInteger bytesWritten;
	return [self writeBytes:&ui16 length:sizeof(uint16_t) bytesWritten:&bytesWritten error:error] && bytesWritten == sizeof(uint16_t);
}

- (BOOL)writeUInt32:(uint32_t)ui32 error:(NSError **)error
{
	NSInteger bytesWritten;
	return [self writeBytes:&ui32 length:sizeof(uint32_t) bytesWritten:&bytesWritten error:error] && bytesWritten == sizeof(uint32_t);
}

- (BOOL)writeUInt64:(uint64_t)ui64 error:(NSError **)error
{
	NSInteger bytesWritten;
	return [self writeBytes:&ui64 length:sizeof(uint64_t) bytesWritten:&bytesWritten error:error] && bytesWritten == sizeof(uint64_t);
}

@end

@implementation SFBOutputSource (SFBBigEndianWriting)
- (BOOL)writeUInt16BigEndian:(uint16_t)ui16 error:(NSError **)error 	{ return [self writeUInt16:OSSwapHostToBigInt16(ui16) error:error]; }
- (BOOL)writeUInt32BigEndian:(uint32_t)ui32 error:(NSError **)error 	{ return [self writeUInt32:OSSwapHostToBigInt32(ui32) error:error]; }
- (BOOL)writeUInt64BigEndian:(uint64_t)ui64 error:(NSError **)error 	{ return [self writeUInt64:OSSwapHostToBigInt64(ui64) error:error]; }
@end

@implementation SFBOutputSource (SFBLittleEndianWriting)
- (BOOL)writeUInt16LittleEndian:(uint16_t)ui16 error:(NSError **)error 	{ return [self writeUInt16:OSSwapHostToLittleInt16(ui16) error:error]; }
- (BOOL)writeUInt32LittleEndian:(uint32_t)ui32 error:(NSError **)error 	{ return [self writeUInt32:OSSwapHostToLittleInt32(ui32) error:error]; }
- (BOOL)writeUInt64LittleEndian:(uint64_t)ui64 error:(NSError **)error 	{ return [self writeUInt64:OSSwapHostToLittleInt64(ui64) error:error]; }
@end
