//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CoreAudio

/// A thin wrapper around a variable-length `AudioBufferList` structure
public class AudioBufferListWrapper {
	/// The underlying memory
	let ptr: UnsafePointer<UInt8>

	/// Creates a new `AudioBufferListWrapper` instance
	/// - note: The returned object assumes ownership of `mem`
	init(_ mem: UnsafePointer<UInt8>) {
		ptr = mem
	}

	deinit {
		ptr.deallocate()
	}

	/// Returns the buffer list's `mNumberBuffers`
	public var numberBuffers: UInt32 {
		return ptr.withMemoryRebound(to: AudioBufferList.self, capacity: 1) { $0.pointee.mNumberBuffers }
	}

	/// Returns the buffer list's `mBuffers`
	public var buffers: UnsafeBufferPointer<AudioBuffer> {
		let count = Int(numberBuffers)
		// Does not compile (!) : MemoryLayout<AudioBufferList>.offset(of: \.mBuffers)
		let offset = MemoryLayout.offset(of: \AudioBufferList.mBuffers)!
		let bufPtr = UnsafeRawPointer(ptr.advanced(by: offset)).assumingMemoryBound(to: AudioBuffer.self)
		return UnsafeBufferPointer<AudioBuffer>(start: bufPtr, count: count)
	}

	/// Performs `block` with a pointer to the underlying `AudioBufferList` structure
	public func withUnsafePointer<T>(_ block: (UnsafePointer<AudioBufferList>) throws -> T) rethrows -> T {
		return try ptr.withMemoryRebound(to: AudioBufferList.self, capacity: 1) { return try block($0) }
	}
}

extension AudioBufferListWrapper: CustomDebugStringConvertible {
	// A textual representation of this instance, suitable for debugging.
	public var debugDescription: String {
		return "<\(type(of: self)): mNumberBuffers = \(numberBuffers), mBuffers = [\(buffers.map({ $0.debugDescription }).joined(separator: ", "))]>"
	}
}

extension AudioBuffer: CustomDebugStringConvertible {
	// A textual representation of this instance, suitable for debugging.
	public var debugDescription: String {
		return "<\(type(of: self)): mNumberChannels = \(mNumberChannels), mDataByteSize = \(mDataByteSize), mData = \(mData?.debugDescription ?? "nil")>"
	}
}
