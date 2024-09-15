//
// Copyright (c) 2010-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "NSFileHandle+SFBHeaderReading.h"

#import "NSData+SFBExtensions.h"

#define ID3V2_TAG_HEADER_LENGTH_BYTES 10
#define ID3V2_TAG_FOOTER_LENGTH_BYTES 10

@implementation NSFileHandle (SFBHeaderReading)

- (NSData *)readHeaderOfLength:(NSUInteger)length skipID3v2Tag:(BOOL)skipID3v2Tag error:(NSError **)error
{
	NSParameterAssert(length > 0);

	unsigned long long originalOffset;
	if(![self getOffset:&originalOffset error:error])
		return nil;

	if(![self seekToOffset:0 error:error])
		return nil;

	if(skipID3v2Tag) {
		NSInteger offset = 0;

		// Attempt to detect and minimally parse an ID3v2 tag header
		NSData *data = [self readDataUpToLength:ID3V2_TAG_HEADER_LENGTH_BYTES error:error];
		if([data startsWithID3v2Header]) {
			const uint8_t *bytes = data.bytes;

			uint8_t flags = bytes[5];
			uint32_t size = (bytes[6] << 21) | (bytes[7] << 14) | (bytes[8] << 7) | bytes[9];

			offset = ID3V2_TAG_HEADER_LENGTH_BYTES + size + (flags & 0x10 ? ID3V2_TAG_FOOTER_LENGTH_BYTES : 0);
		}

		if(![self seekToOffset:offset error:error])
			return nil;
	}

	NSData *data = [self readDataUpToLength:length error:error];
	if(!data)
		return nil;

	if(data.length < length) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
		return nil;
	}

	if(![self seekToOffset:originalOffset error:error])
		return nil;

	return data;
}

@end
