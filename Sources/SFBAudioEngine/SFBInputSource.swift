//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension InputSource {
	/// Reads bytes from the input
	/// - parameter buffer: A buffer to receive data
	/// - parameter length: The maximum number of bytes to read
	/// - returns: The number of bytes actually read
	/// - throws: An `NSError` object if an error occurs
	public func read(_ buffer: UnsafeMutableRawPointer, length: Int) throws -> Int {
		var bytesRead = 0
		try __readBytes(buffer, length: length, bytesRead: &bytesRead)
		return bytesRead
	}

	/// The current offset in the input, in bytes
	/// - throws: An `NSError` object if an error occurs
	public var offset: Int {
		get throws {
			var offset = 0
			try __getOffset(&offset)
			return offset
		}
	}

	/// The length of the input, in bytes
	/// - throws: An `NSError` object if an error occurs
	public var length: Int {
		get throws {
			var length = 0
			try __getLength(&length)
			return length
		}
	}

	/// Reads and returns a binary integer
	/// - returns: The integer value read
	/// - throws: An `NSError` object if an error occurs
	public func read<T: BinaryInteger>() throws -> T {
		var i: T = 0
		let size = MemoryLayout<T>.size

		let bytesRead = try withUnsafeMutablePointer(to: &i) {
			return try read($0, length: size)
		}

		if bytesRead != size {
			throw NSError(domain: NSPOSIXErrorDomain, code: Int(EIO), userInfo: nil)
		}

		return i
	}

	/// Reads a 16-bit integer from the input in big-endian format and returns the value
	/// - throws: An `NSError` object if an error occurs
	public func readBigEndian() throws -> UInt16 {
		let ui16: UInt16 = try read()
		return CFSwapInt16HostToBig(ui16)
	}

	/// Reads a 32-bit integer from the input in big-endian format and returns the value
	/// - throws: An `NSError` object if an error occurs
	public func readBigEndian() throws -> UInt32 {
		let ui32: UInt32 = try read()
		return CFSwapInt32HostToBig(ui32)
	}

	/// Reads a 64-bit integer from the input in big-endian format and returns the value
	/// - throws: An `NSError` object if an error occurs
	public func readBigEndian() throws -> UInt64 {
		let ui64: UInt64 = try read()
		return CFSwapInt64HostToBig(ui64)
	}

	/// Reads a 16-bit integer from the input in little-endian format and returns the value
	/// - throws: An `NSError` object if an error occurs
	public func readLittleEndian() throws -> UInt16 {
		let ui16: UInt16 = try read()
		return CFSwapInt16HostToLittle(ui16)
	}

	/// Reads a 32-bit integer from the input in little-endian format and returns the value
	/// - throws: An `NSError` object if an error occurs
	public func readLittleEndian() throws -> UInt32 {
		let ui32: UInt32 = try read()
		return CFSwapInt32HostToLittle(ui32)
	}

	/// Reads a 64-bit integer from the input in little-endian format and returns the value
	/// - throws: An `NSError` object if an error occurs
	public func readLittleEndian() throws -> UInt64 {
		let ui64: UInt64 = try read()
		return CFSwapInt64HostToLittle(ui64)
	}
}
