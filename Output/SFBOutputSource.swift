/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension OutputSource {
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

	/// Writes bytes to the output
	/// - parameter buffer: A buffer of data to write
	/// - parameter length: The maximum number of bytes to write
	/// - returns:The number of bytes actually written
	/// - throws: An `NSError` object if an error occurs
	public func write(_ buffer: UnsafeRawPointer, length: Int) throws -> Int {
		var bytesWritten = 0
		try __writeBytes(buffer, length: length, bytesWritten: &bytesWritten)
		return bytesWritten
	}

	/// Returns the current offset in the output, in bytes
	/// - throws: An `NSError` object if an error occurs
	public func offset() throws -> Int {
		var offset = 0
		try __getOffset(&offset)
		return offset
	}

	/// Returns the length of the output, in bytes
	/// - throws: An `NSError` object if an error occurs
	public func length() throws -> Int {
		var length = 0
		try __getLength(&length)
		return length
	}

	/// Writes bytes to the output
	/// - parameter data: The data to write
	/// - throws: An `NSError` object if an error occurs
	public func write(_ data: Data) throws {
		let bytesWritten = try data.withUnsafeBytes { (bufptr) -> Int in
			guard let baseAddress = bufptr.baseAddress else {
				return 0
			}
			return try write(baseAddress, length: data.count)
		}

		if bytesWritten != data.count {
			throw NSError(domain: NSPOSIXErrorDomain, code: Int(EIO), userInfo: nil)
		}
	}

	/// Writes a binary integer to the output
	/// - parameter i: The value to write
	/// - throws: An `NSError` object if an error occurs
	public func write<T: BinaryInteger>(_ i: T) throws {
		let size = MemoryLayout<T>.size

		var tmp = i
		let bytesWritten = try write(&tmp, length: size)

		if bytesWritten != size {
			throw NSError(domain: NSPOSIXErrorDomain, code: Int(EIO), userInfo: nil)
		}
	}

	/// Writes a 16-bit integer to the output in big-endian format
	/// - parameter ui16: The value to write
	/// - throws: An `NSError` object if an error occurs
	public func writeBigEndian(_ ui16: UInt16) throws {
		try write(CFSwapInt16HostToBig(ui16))
	}

	/// Writes a 32-bit integer to the output in big-endian format
	/// - parameter ui32: The value to write
	/// - throws: An `NSError` object if an error occurs
	public func writeBigEndian(_ ui32: UInt32) throws {
		try write(CFSwapInt32HostToBig(ui32))
	}

	/// Writes a 64-bit integer to the output in little-endian format
	/// - parameter ui64: The value to write
	/// - throws: An `NSError` object if an error occurs
	public func writeBigEndian(_ ui64: UInt64) throws {
		try write(CFSwapInt64HostToBig(ui64))
	}

	/// Writes a 16-bit integer to the output in big-endian format
	/// - parameter ui16: The value to write
	/// - throws: An `NSError` object if an error occurs
	public func writeLittleEndian(_ ui16: UInt16) throws {
		try write(CFSwapInt16HostToLittle(ui16))
	}

	/// Writes a 32-bit integer to the output in little-endian format
	/// - parameter ui32: The value to write
	/// - throws: An `NSError` object if an error occurs
	public func writeLittleEndian(_ ui32: UInt32) throws {
		try write(CFSwapInt32HostToLittle(ui32))
	}

	/// Writes a 64-bit integer to the output in little-endian format
	/// - parameter ui64: The value to write
	/// - throws: An `NSError` object if an error occurs
	public func writeLittleEndian(_ ui64: UInt64) throws {
		try write(CFSwapInt64HostToLittle(ui64))
	}

}
