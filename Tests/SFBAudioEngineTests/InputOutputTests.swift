//
// SPDX-FileCopyrightText: 2012 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import Testing
@testable import SFBAudioEngine

@Suite struct SFBAudioEngineTests {
    @Test func inputSourceFromData() throws {
        let input = InputSource(data: Data(repeating: 0xfe, count: 16))
        #expect(input.isOpen)
        #expect(input.supportsSeeking)
        #expect(try input.offset == 0)
        let i: UInt8 = try input.read()
        #expect(i == 0xfe)
        #expect(try input.offset == 1)
        #expect(try input.length == 16)
    }

    @Test func outputTargetFromData() throws {
        let output = OutputTarget.makeForData()
        #expect(output.isOpen)
        #expect(output.supportsSeeking)
        var i: UInt32 = 0x12345678
        #expect(try output.write(&i, length: MemoryLayout<UInt32>.size) == MemoryLayout<UInt32>.size)
        try output.seek(toOffset: 0)
        #expect(try output.read(&i, length: MemoryLayout<UInt32>.size) == MemoryLayout<UInt32>.size)
        #expect(i == 0x12345678)
    }
}
