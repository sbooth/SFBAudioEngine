//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "NSData+SFBExtensions.h"

@implementation NSData (SFBNumericValueMethods)

- (uint32_t)uint32AtLocation:(NSUInteger)location
{
	uint32_t ui32;
	[self getBytes:&ui32 range:NSMakeRange(location, 4)];
	return ui32;
}

- (uint32_t)uint32BigEndianAtLocation:(NSUInteger)location
{
	uint32_t ui32 = [self uint32AtLocation:location];
	return OSSwapHostToBigInt32(ui32);
}

- (uint32_t)uint32LittleEndianAtLocation:(NSUInteger)location
{
	uint32_t ui32 = [self uint32AtLocation:location];
	return OSSwapHostToLittleInt32(ui32);
}

@end


@implementation NSData (SFBMatchMethods)

- (BOOL)startsWith:(NSData *)pattern
{
	return [self containsBytes:pattern.bytes length:pattern.length atLocation:0];
}

- (BOOL)startsWithBytes:(const void *)patternBytes length:(NSUInteger)patternLength
{
	return [self containsBytes:patternBytes length:patternLength atLocation:0];
}

- (BOOL)contains:(NSData *)pattern atLocation:(NSUInteger)location
{
	return [self containsBytes:pattern.bytes length:pattern.length atLocation:location];
}

- (BOOL)containsBytes:(const void *)patternBytes length:(NSUInteger)patternLength atLocation:(NSUInteger)location
{
	NSParameterAssert(patternBytes != NULL);

	NSUInteger length = self.length;
	NSParameterAssert(location < length);

	return !memcmp((const uint8_t *)self.bytes + location, patternBytes, patternLength);
}

@end


@implementation NSData (SFBSearchMethods)

- (BOOL)contains:(NSData *)pattern
{
	return [self findBytes:pattern.bytes length:pattern.length startingLocation:0] != NSNotFound;
}

- (BOOL)contains:(NSData *)pattern searchingFromLocation:(NSUInteger)startingLocation
{
	return [self findBytes:pattern.bytes length:pattern.length startingLocation:startingLocation] != NSNotFound;
}

- (BOOL)containsBytes:(const void *)patternBytes length:(NSUInteger)patternLength
{
	return [self findBytes:patternBytes length:patternLength startingLocation:0] != NSNotFound;
}

- (BOOL)containsBytes:(const void *)patternBytes length:(NSUInteger)patternLength searchingFromLocation:(NSUInteger)startingLocation
{
	return [self findBytes:patternBytes length:patternLength startingLocation:startingLocation] != NSNotFound;
}


- (NSUInteger)find:(NSData *)pattern
{
	return [self findBytes:pattern.bytes length:pattern.length startingLocation:0];
}

- (NSUInteger)find:(NSData *)pattern startingLocation:(NSUInteger)startingLocation
{
	return [self findBytes:pattern.bytes length:pattern.length startingLocation:startingLocation];
}

- (NSUInteger)findBytes:(const void *)patternBytes length:(NSUInteger)patternLength
{
	return [self findBytes:patternBytes length:patternLength startingLocation:0];
}

- (NSUInteger)findBytes:(const void *)patternBytes length:(NSUInteger)patternLength startingLocation:(NSUInteger)startingLocation
{
	NSParameterAssert(patternBytes != NULL);

	NSUInteger length = self.length;
	NSParameterAssert(startingLocation < length);

	const void *bytes = (const uint8_t *)self.bytes + startingLocation;
	const void *offset = memmem(bytes, length - startingLocation, patternBytes, patternLength);
	if(offset)
		return offset - bytes;
	return NSNotFound;
}

@end


@implementation NSData (SFBID3v2Methods)

- (BOOL)startsWithID3v2Header
{
	if(self.length < 10)
		return NO;

	/*
	 An ID3v2 tag can be detected with the following pattern:
	 $49 44 33 yy yy xx zz zz zz zz
	 Where yy is less than $FF, xx is the 'flags' byte and zz is less than
	 $80.
	 */

	const uint8_t *bytes = self.bytes;
	if(bytes[0] != 0x49 || bytes[1] != 0x44 || bytes[2] != 0x33)
		return NO;
	if(bytes[3] >= 0xff || bytes[4] >= 0xff)
		return NO;
	if(bytes[5] & 0xf)
		return NO;
	if(bytes[6] >= 0x80 || bytes[7] >= 0x80 || bytes[8] >= 0x80 || bytes[9] >= 0x80)
		return NO;
	return YES;
}

@end
