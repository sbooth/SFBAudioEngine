//
// Copyright (c) 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <XCTest/XCTest.h>

#import "SFBByteStream.hpp"

@interface Tests : XCTestCase

@end

@implementation Tests

- (void)testByteStream {
	auto bs = SFB::ByteStream();
	XCTAssert(bs.Length() == 0);
	XCTAssert(bs.Position() == 0);
	XCTAssert(bs.Remaining() == 0);

	XCTAssertThrows(SFB::ByteStream(nullptr, 1));

	uint8 buf[] = { 'a', 'b', 'c', 1, 2, 3, 0xde, 0xad, 0xbe, 0xef };
	bs = SFB::ByteStream(buf, 10);
	XCTAssert(bs.Length() == 10);
	XCTAssert(bs.Position() == 0);
	XCTAssert(bs.Remaining() == 10);

	uint16_t ui16;
	XCTAssertTrue(bs.ReadLE(ui16));
	XCTAssert(ui16 == 0x6261);
	XCTAssert(bs.Position() == 2);
	XCTAssert(bs.Remaining() == 8);

	XCTAssert(bs.Rewind(2) == 2);
	XCTAssert(bs.Position() == 0);
	XCTAssert(bs.Remaining() == 10);

	XCTAssertTrue(bs.ReadBE(ui16));
	XCTAssert(ui16 == 0x6162);
	XCTAssert(bs.Position() == 2);
	XCTAssert(bs.Remaining() == 8);

	XCTAssert(bs.SetPosition(0) == 0);
	XCTAssert(bs.Skip(1) == 1);
	XCTAssert(bs.Position() == 1);
	XCTAssert(bs.Remaining() == 9);

	uint64_t ui64;
	XCTAssertTrue(bs.ReadLE(ui64));
	XCTAssert(ui64 == 0xBEADDE0302016362);
	XCTAssert(bs.Position() == 9);
	XCTAssert(bs.Remaining() == 1);

	uint64_t buf2[] = { 0xdecafbadbaddecaf, 0xdeadbeefbeefdead};
	bs = SFB::ByteStream(buf2, 16);
	XCTAssert(bs.Length() == 16);
	XCTAssert(bs.Position() == 0);
	XCTAssert(bs.Remaining() == 16);

	uint32_t ui32;
	XCTAssertTrue(bs.Read(ui32));
	XCTAssert(ui32 == 0xbaddecaf);
	XCTAssert(bs.Position() == 4);
	XCTAssert(bs.Remaining() == 12);

	XCTAssert(bs.SetPosition(0) == 0);

	XCTAssertTrue(bs.ReadLE(ui32));
	XCTAssert(ui32 == 0xbaddecaf);
	XCTAssert(bs.Position() == 4);
	XCTAssert(bs.Remaining() == 12);

	XCTAssert(bs.SetPosition(0) == 0);

	XCTAssertTrue(bs.ReadBE(ui32));
	XCTAssert(ui32 == 0xafecddba);
	XCTAssert(bs.Position() == 4);
	XCTAssert(bs.Remaining() == 12);

	char s[4];
	XCTAssertTrue(bs.Read(s, 4));
	XCTAssert(!std::strncmp(s, "\xad\xfb\xca\xde", 4));
	XCTAssert(bs.Position() == 8);
	XCTAssert(bs.Remaining() == 8);

	XCTAssert(bs.Rewind(4) == 4);
	XCTAssert(bs.Position() == 4);
	XCTAssert(bs.Remaining() == 12);

	XCTAssert(bs.Read(&s[3], 1) == 1);
	XCTAssert(bs.Read(&s[2], 1) == 1);
	XCTAssert(bs.Read(&s[1], 1) == 1);
	XCTAssert(bs.Read(&s[0], 1) == 1);
	XCTAssert(!std::strncmp(s, "\xde\xca\xfb\xad", 4));
}

@end
