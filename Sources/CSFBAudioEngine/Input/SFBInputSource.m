//
// Copyright (c) 2010-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBInputSource.h"
#import "SFBInputSource+Internal.h"

#import "SFBDataInputSource.h"
#import "SFBFileContentsInputSource.h"
#import "SFBFileInputSource.h"
#import "SFBMemoryMappedFileInputSource.h"

#import "NSData+SFBExtensions.h"

// NSError domain for InputSource and subclasses
NSErrorDomain const SFBInputSourceErrorDomain = @"org.sbooth.AudioEngine.InputSource";

os_log_t gSFBInputSourceLog = NULL;

static void SFBCreateInputSourceLog(void) __attribute__ ((constructor));
static void SFBCreateInputSourceLog(void)
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBInputSourceLog = os_log_create("org.sbooth.AudioEngine", "InputSource");
	});
}

@implementation SFBInputSource

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBInputSourceErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if(userInfoKey == NSLocalizedDescriptionKey) {
			switch(err.code) {
				case SFBInputSourceErrorCodeFileNotFound:
					return NSLocalizedString(@"The requested file was not found.", @"");
				case SFBInputSourceErrorCodeInputOutput:
					return NSLocalizedString(@"An input/output error occurred.", @"");
				case SFBInputSourceErrorCodeNotSeekable:
					return NSLocalizedString(@"The input does not support seeking.", @"");
			}
		}
		return nil;
	}];
}

+ (instancetype)inputSourceForURL:(NSURL *)url error:(NSError **)error
{
	return [SFBInputSource inputSourceForURL:url flags:0 error:error];
}

+ (instancetype)inputSourceForURL:(NSURL *)url flags:(SFBInputSourceFlags)flags error:(NSError **)error
{
	NSParameterAssert(url != nil);
	NSParameterAssert(url.isFileURL);

	if(flags & SFBInputSourceFlagsMemoryMapFiles)
		return [[SFBMemoryMappedFileInputSource alloc] initWithURL:url error:error];
	else if(flags & SFBInputSourceFlagsLoadFilesInMemory)
		return [[SFBFileContentsInputSource alloc] initWithContentsOfURL:url error:error];
	else
		return [[SFBFileInputSource alloc] initWithURL:url error:error];

	return nil;
}

+ (instancetype)inputSourceWithData:(NSData *)data
{
	NSParameterAssert(data != nil);
	return [[SFBDataInputSource alloc] initWithData:data];
}

+ (instancetype)inputSourceWithBytes:(const void *)bytes length:(NSInteger)length
{
	NSParameterAssert(bytes != NULL);
	NSParameterAssert(length >= 0);
	NSData *data = [NSData dataWithBytes:bytes length:(NSUInteger)length];
	if(data == nil)
		return nil;
	return [[SFBDataInputSource alloc] initWithData:data];
}

+ (instancetype)inputSourceWithBytesNoCopy:(void *)bytes length:(NSInteger)length freeWhenDone:(BOOL)freeWhenDone
{
	NSParameterAssert(bytes != NULL);
	NSParameterAssert(length >= 0);
	NSData *data = [NSData dataWithBytesNoCopy:bytes length:(NSUInteger)length freeWhenDone:freeWhenDone];
	if(data == nil)
		return nil;
	return [[SFBDataInputSource alloc] initWithData:data];
}

- (void)dealloc
{
	if(self.isOpen)
		[self closeReturningError:nil];
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

- (NSString *)description
{
	if(_url)
		return [NSString stringWithFormat:@"<%@ %p: \"%@\">", [self class], self, [[NSFileManager defaultManager] displayNameAtPath:_url.path]];
	else
		return [NSString stringWithFormat:@"<%@ %p>", [self class], self];
}

@end

@implementation SFBInputSource (SFBSignedIntegerReading)
- (BOOL)readInt8:(int8_t *)i8 error:(NSError **)error			{ return [self readUInt8:(uint8_t *)i8 error:error]; }
- (BOOL)readInt16:(int16_t *)i16 error:(NSError **)error		{ return [self readUInt16:(uint16_t *)i16 error:error]; }
- (BOOL)readInt32:(int32_t *)i32 error:(NSError **)error		{ return [self readUInt32:(uint32_t *)i32 error:error]; }
- (BOOL)readInt64:(int64_t *)i64 error:(NSError **)error		{ return [self readUInt64:(uint64_t *)i64 error:error]; }
@end

@implementation SFBInputSource (SFBUnsignedIntegerReading)
- (BOOL)readUInt8:(uint8_t *)ui8 error:(NSError **)error
{
	NSInteger bytesRead;
	return [self readBytes:ui8 length:sizeof(uint8_t) bytesRead:&bytesRead error:error] && bytesRead == sizeof(uint8_t);
}

- (BOOL)readUInt16:(uint16_t *)ui16 error:(NSError **)error
{
	NSInteger bytesRead;
	return [self readBytes:ui16 length:sizeof(uint16_t) bytesRead:&bytesRead error:error] && bytesRead == sizeof(uint16_t);
}

- (BOOL)readUInt32:(uint32_t *)ui32 error:(NSError **)error
{
	NSInteger bytesRead;
	return [self readBytes:ui32 length:sizeof(uint32_t) bytesRead:&bytesRead error:error] && bytesRead == sizeof(uint32_t);
}

- (BOOL)readUInt64:(uint64_t *)ui64 error:(NSError **)error
{
	NSInteger bytesRead;
	return [self readBytes:ui64 length:sizeof(uint64_t) bytesRead:&bytesRead error:error] && bytesRead == sizeof(uint64_t);
}

@end

@implementation SFBInputSource (SFBBigEndianReading)

- (BOOL)readUInt16BigEndian:(uint16_t *)ui16 error:(NSError **)error
{
	NSParameterAssert(ui16 != nil);
	if(![self readUInt16:ui16 error:error])
		return NO;
	*ui16 = OSSwapHostToBigInt16(*ui16);
	return YES;
}

- (BOOL)readUInt32BigEndian:(uint32_t *)ui32 error:(NSError **)error
{
	NSParameterAssert(ui32 != nil);
	if(![self readUInt32:ui32 error:error])
		return NO;
	*ui32 = OSSwapHostToBigInt32(*ui32);
	return YES;
}

- (BOOL)readUInt64BigEndian:(uint64_t *)ui64 error:(NSError **)error
{
	NSParameterAssert(ui64 != nil);
	if(![self readUInt64:ui64 error:error])
		return NO;
	*ui64 = OSSwapHostToBigInt64(*ui64);
	return YES;
}

@end

@implementation SFBInputSource (SFBLittleEndianReading)

- (BOOL)readUInt16LittleEndian:(uint16_t *)ui16 error:(NSError **)error
{
	NSParameterAssert(ui16 != nil);
	if(![self readUInt16:ui16 error:error])
		return NO;
	*ui16 = OSSwapHostToLittleInt16(*ui16);
	return YES;
}

- (BOOL)readUInt32LittleEndian:(uint32_t *)ui32 error:(NSError **)error
{
	NSParameterAssert(ui32 != nil);
	if(![self readUInt32:ui32 error:error])
		return NO;
	*ui32 = OSSwapHostToLittleInt32(*ui32);
	return YES;
}

- (BOOL)readUInt64LittleEndian:(uint64_t *)ui64 error:(NSError **)error
{
	NSParameterAssert(ui64 != nil);
	if(![self readUInt64:ui64 error:error])
		return NO;
	*ui64 = OSSwapHostToLittleInt64(*ui64);
	return YES;
}

@end

@implementation SFBInputSource (SFBDataReading)

- (NSData *)readDataOfLength:(NSUInteger)length error:(NSError **)error
{
	if(length == 0)
		return [NSData data];

	void *buf = malloc(length);
	if(!buf) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return nil;
	}

	NSInteger bytesRead = 0;
	if(![self readBytes:buf length:length bytesRead:&bytesRead error:error]) {
		free(buf);
		return nil;
	}

	return [NSData dataWithBytesNoCopy:buf length:bytesRead freeWhenDone:YES];
}

@end

#define ID3V2_TAG_HEADER_LENGTH_BYTES 10
#define ID3V2_TAG_FOOTER_LENGTH_BYTES 10

@implementation SFBInputSource (SFBHeaderReading)

- (NSData *)readHeaderOfLength:(NSUInteger)length skipID3v2Tag:(BOOL)skipID3v2Tag error:(NSError **)error
{
	NSParameterAssert(length > 0);

	if(!self.supportsSeeking) {
		if(error)
			*error = [NSError errorWithDomain:SFBInputSourceErrorDomain code:SFBInputSourceErrorCodeNotSeekable userInfo:nil];
		return nil;
	}

	NSInteger originalOffset;
	if(![self getOffset:&originalOffset error:error])
		return nil;

	if(![self seekToOffset:0 error:error])
		return nil;

	if(skipID3v2Tag) {
		NSInteger offset = 0;

		// Attempt to detect and minimally parse an ID3v2 tag header
		NSData *data = [self readDataOfLength:ID3V2_TAG_HEADER_LENGTH_BYTES error:error];
		if([data startsWithID3v2Header]) {
			const uint8_t *bytes = data.bytes;

			uint8_t flags = bytes[5];
			uint32_t size = (bytes[6] << 21) | (bytes[7] << 14) | (bytes[8] << 7) | bytes[9];

			offset = ID3V2_TAG_HEADER_LENGTH_BYTES + size + (flags & 0x10 ? ID3V2_TAG_FOOTER_LENGTH_BYTES : 0);
		}

		if(![self seekToOffset:offset error:error])
			return nil;
	}

	NSData *data = [self readDataOfLength:length error:error];
	if(!data || data.length < length)
		return nil;

	if(![self seekToOffset:originalOffset error:error])
		return nil;

	return data;
}

@end
