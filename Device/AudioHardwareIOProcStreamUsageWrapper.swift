//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CoreAudio

extension AudioHardwareIOProcStreamUsage {
	/// Returns the size in bytes of an `AudioHardwareIOProcStreamUsage` struct
	/// - parameter maximumStreams: The number of streams
	public static func sizeInBytes(maximumStreams: Int) -> Int {
		let offset = MemoryLayout.offset(of: \AudioHardwareIOProcStreamUsage.mStreamIsOn)!
		return offset + (MemoryLayout<UInt32>.stride * maximumStreams)
	}
}

/// A thin wrapper around a variable-length `AudioHardwareIOProcStreamUsage` structure
public class AudioHardwareIOProcStreamUsageWrapper {
	/// The underlying memory
	let ptr: UnsafePointer<UInt8>

	/// Creates a new `AudioHardwareIOProcStreamUsage` instance
	/// - note: The returned object assumes ownership of `mem`
	init(_ mem: UnsafePointer<UInt8>) {
		ptr = mem
	}

	deinit {
		ptr.deallocate()
	}

	/// Returns the stream usage's `mIOProc`
	public var ioProc: UnsafeRawPointer {
		return ptr.withMemoryRebound(to: AudioHardwareIOProcStreamUsage.self, capacity: 1) { UnsafeRawPointer($0.pointee.mIOProc) }
	}

	/// Returns the stream usage's `mNumberStreams`
	public var numberStreams: UInt32 {
		return ptr.withMemoryRebound(to: AudioHardwareIOProcStreamUsage.self, capacity: 1) { $0.pointee.mNumberStreams }
	}

	/// Returns the stream usage's `mStreamIsOn`
	public var streamIsOn: UnsafeBufferPointer<UInt32> {
		let count = Int(numberStreams)
		// Does not compile (!) : MemoryLayout<AudioHardwareIOProcStreamUsage>.offset(of: \.mStreamIsOn)
		let offset = MemoryLayout.offset(of: \AudioHardwareIOProcStreamUsage.mStreamIsOn)!
		let bufPtr = UnsafeRawPointer(ptr.advanced(by: offset)).assumingMemoryBound(to: UInt32.self)
		return UnsafeBufferPointer<UInt32>(start: bufPtr, count: count)
	}

	/// Performs `block` with a pointer to the underlying `AudioHardwareIOProcStreamUsage` structure
	public func withUnsafePointer<T>(_ block: (UnsafePointer<AudioHardwareIOProcStreamUsage>) throws -> T) rethrows -> T {
		return try ptr.withMemoryRebound(to: AudioHardwareIOProcStreamUsage.self, capacity: 1) { return try block($0) }
	}
}

extension AudioHardwareIOProcStreamUsageWrapper: CustomDebugStringConvertible {
	// A textual representation of this instance, suitable for debugging.
	public var debugDescription: String {
		return "<\(type(of: self)): mNumberStreams = \(numberStreams), mStreamIsOn = [\(streamIsOn.map({ $0 == 0 ? "Off" : "On" }).joined(separator: ", "))]>"
	}
}
