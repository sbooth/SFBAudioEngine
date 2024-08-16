//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "NSData+SFBExtensions.h"

@implementation NSData (SFBMatchMethods)

- (BOOL)startsWith:(NSData *)pattern
{
	return [self matchesBytes:pattern.bytes length:pattern.length atLocation:0];
}

- (BOOL)startsWithBytes:(const void *)bytes length:(NSUInteger)length
{
	return [self matchesBytes:bytes length:length atLocation:0];
}


- (BOOL)matches:(NSData *)pattern atLocation:(NSUInteger)location
{
	return [self matchesBytes:pattern.bytes length:pattern.length atLocation:location];
}

- (BOOL)matchesBytes:(const void *)bytes length:(NSUInteger)length atLocation:(NSUInteger)location
{
	NSParameterAssert(bytes != NULL);
	NSParameterAssert(location < self.length);

	return !memcmp((const uint8_t *)self.bytes + location, bytes, length);
}

@end


@implementation NSData (SFBSearchMethods)

- (BOOL)contains:(NSData *)pattern
{
	return [self findBytes:pattern.bytes length:pattern.length searchingFromLocation:0] != NSNotFound;
}

- (BOOL)contains:(NSData *)pattern searchingFromLocation:(NSUInteger)location
{
	return [self findBytes:pattern.bytes length:pattern.length searchingFromLocation:location] != NSNotFound;
}

- (BOOL)containsBytes:(const void *)bytes length:(NSUInteger)length
{
	return [self findBytes:bytes length:length searchingFromLocation:0] != NSNotFound;
}

- (BOOL)containsBytes:(const void *)bytes length:(NSUInteger)length searchingFromLocation:(NSUInteger)location
{
	return [self findBytes:bytes length:length searchingFromLocation:location] != NSNotFound;
}


- (NSUInteger)find:(NSData *)pattern
{
	return [self findBytes:pattern.bytes length:pattern.length searchingFromLocation:0];
}

- (NSUInteger)find:(NSData *)pattern searchingFromLocation:(NSUInteger)location
{
	return [self findBytes:pattern.bytes length:pattern.length searchingFromLocation:location];
}

- (NSUInteger)findBytes:(const void *)bytes length:(NSUInteger)length
{
	return [self findBytes:bytes length:length searchingFromLocation:0];
}

- (NSUInteger)findBytes:(const void *)bytes length:(NSUInteger)length searchingFromLocation:(NSUInteger)location
{
	NSParameterAssert(bytes != NULL);

	NSUInteger len = self.length;
	NSParameterAssert(location < len);

	const void *buf = (const uint8_t *)self.bytes + location;
	const void *offset = memmem(buf, len - location, bytes, length);
	if(offset)
		return offset - buf;
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
