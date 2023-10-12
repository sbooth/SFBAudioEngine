//
// Copyright (c) 2020 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CoreAudioTypes
@_implementationOnly import CoreAudioUtilities

/// A thin wrapper around a variable-length `AudioChannelLayout` structure
public struct AudioChannelLayoutWrapper {
	/// The underlying memory
	let ptr: UnsafePointer<UInt8>

	/// Creates a new `AudioChannelLayoutWrapper` instance wrapping `mem`
	init(_ mem: UnsafePointer<UInt8>) {
		ptr = mem
	}

	/// Returns the number of channels in the layout
	public var channelCount: UInt32 {
		return ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { $0.pointee.channelCount }
	}

	/// Returns the layout's `mAudioChannelLayoutTag`
	public var tag: AudioChannelLayoutTag {
		return ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { $0.pointee.mChannelLayoutTag }
	}

	/// Returns the layout's `mAudioChannelBitmap`
	public var bitmap: AudioChannelBitmap {
		return ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { $0.pointee.mChannelBitmap }
	}

	/// Returns the layout's `mNumberChannelDescriptions`
	public var numberChannelDescriptions: UInt32 {
		return ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { $0.pointee.mNumberChannelDescriptions }
	}

	/// Returns the layout's `mChannelDescriptions`
	public var channelDescriptions: UnsafeBufferPointer<AudioChannelDescription> {
		let count = Int(numberChannelDescriptions)
		// Does not compile (!) : MemoryLayout<AudioChannelLayout>.offset(of: \.mChannelDescriptions)
		let offset = MemoryLayout.offset(of: \AudioChannelLayout.mChannelDescriptions)!
		let chanPtr = UnsafeRawPointer(ptr.advanced(by: offset)).assumingMemoryBound(to: AudioChannelDescription.self)
		return UnsafeBufferPointer<AudioChannelDescription>(start: chanPtr, count: count)
	}

	/// Performs `block` with a pointer to the underlying `AudioChannelLayout` structure
	public func withUnsafePointer<T>(_ block: (UnsafePointer<AudioChannelLayout>) throws -> T) rethrows -> T {
		return try ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { return try block($0) }
	}
}

import AVFAudio

extension AudioChannelLayoutWrapper {
	/// Returns `self` converted to an `AVAudioChannelLayout`object
	public var avAudioChannelLayout: AVAudioChannelLayout {
		return withUnsafePointer { return AVAudioChannelLayout(layout: $0) }
	}
}

extension AudioChannelLayoutWrapper: CustomDebugStringConvertible {
	// A textual representation of this instance, suitable for debugging.
	public var debugDescription: String {
		let channelLayoutTag = tag
		if channelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions {
			return "<\(type(of: self)): \(channelCount) ch, [\(channelDescriptions.map({ $0.channelDescription }).joined(separator: ", "))]>"
		}
		else if channelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap {
			return "<\(type(of: self)): \(channelCount) ch, \(bitmap.bitmapDescription)>"
		}
		else {
			return "<\(type(of: self)): \(channelCount) ch, \(channelLayoutTag.channelLayoutTagName)>"
		}
	}
}
