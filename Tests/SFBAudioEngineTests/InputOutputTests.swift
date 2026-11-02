//
// SPDX-FileCopyrightText: 2012 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import XCTest
@testable import SFBAudioEngine

final class SFBAudioEngineTests: XCTestCase {
    func testInputSourceFromData() throws {
        let input = InputSource(data: Data(repeating: 0xfe, count: 16))
        XCTAssertEqual(input.isOpen, true)
        XCTAssertEqual(input.supportsSeeking, true)
        XCTAssertEqual(try input.offset, 0)
        let i: UInt8 = try input.read()
        XCTAssertEqual(i, 0xfe)
        XCTAssertEqual(try input.offset, 1)
        XCTAssertEqual(try input.length, 16)
    }

    func testOutputTargetFromData() throws {
        let output = OutputTarget.makeForData()
        XCTAssertEqual(output.isOpen, true)
        XCTAssertEqual(output.supportsSeeking, true)
        var i: UInt32 = 0x12345678
        XCTAssertEqual(try output.write(&i, length: MemoryLayout<UInt32>.size), MemoryLayout<UInt32>.size)
        try output.seek(toOffset: 0)
        XCTAssertEqual(try output.read(&i, length: MemoryLayout<UInt32>.size), MemoryLayout<UInt32>.size)
        XCTAssertEqual(i, 0x12345678)
    }
}
