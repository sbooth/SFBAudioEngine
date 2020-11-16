/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension OutputSource {
	public func offset() throws -> Int {
		var offset = 0
		try __getOffset(&offset)
		return offset
	}

	public func length() throws -> Int {
		var length = 0
		try __getLength(&length)
		return length
	}

	public func read(_ buffer: UnsafeMutableRawPointer, length: Int) throws -> Int {
		var bytesRead = 0
		try __readBytes(buffer, length: length, bytesRead: &bytesRead)
		return bytesRead
	}

	public func write(_ buffer: UnsafeRawPointer, length: Int) throws -> Int {
		var bytesWritten = 0
		try __writeBytes(buffer, length: length, bytesWritten: &bytesWritten)
		return bytesWritten
	}

	public func write<T: BinaryInteger>(_ i: T) throws
	{
		let size = MemoryLayout<T>.size

		var tmp = i
		let bytesWritten = try write(&tmp, length: size)

		if bytesWritten != size {
			throw NSError(domain: NSPOSIXErrorDomain, code: Int(EIO), userInfo: nil)
		}
	}

	public func writeBigEndian(_ ui16: UInt16) throws
	{
		try write(CFSwapInt16HostToBig(ui16))
	}

	public func writeBigEndian(_ ui32: UInt32) throws
	{
		try write(CFSwapInt32HostToBig(ui32))
	}

	public func writeBigEndian(_ ui64: UInt64) throws
	{
		try write(CFSwapInt64HostToBig(ui64))
	}

	public func writeLittleEndian(_ ui16: UInt16) throws
	{
		try write(CFSwapInt16HostToLittle(ui16))
	}

	public func writeLittleEndian(_ ui32: UInt32) throws
	{
		try write(CFSwapInt32HostToLittle(ui32))
	}

	public func writeLittleEndian(_ ui64: UInt64) throws
	{
		try write(CFSwapInt64HostToLittle(ui64))
	}

}
