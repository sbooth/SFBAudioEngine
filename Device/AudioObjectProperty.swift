//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio
import os.log

/// A thin wrapper around a HAL audio object property selector
public struct PropertySelector: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	public let rawValue: AudioObjectPropertySelector

	public init(rawValue: AudioObjectPropertySelector) {
		self.rawValue = rawValue
	}

	public init(integerLiteral value: UInt32) {
		self.rawValue = value
	}

	public init(stringLiteral value: StringLiteralType) {
		self.rawValue = value.fourCC
	}
}

extension PropertySelector {
	/// Wildcard selector
	public static let wildcard = PropertySelector(rawValue: kAudioObjectPropertySelectorWildcard)
}

/// A thin wrapper around a HAL audio object property scope
public struct PropertyScope: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	public let rawValue: AudioObjectPropertyScope

	public init(rawValue: AudioObjectPropertyScope) {
		self.rawValue = rawValue
	}

	public init(integerLiteral value: UInt32) {
		self.rawValue = value
	}

	public init(stringLiteral value: StringLiteralType) {
		self.rawValue = value.fourCC
	}
}

extension PropertyScope {
	/// Global scope
	public static let global 		= PropertyScope(rawValue: kAudioObjectPropertyScopeGlobal)
	/// Input scope
	public static let input 		= PropertyScope(rawValue: kAudioObjectPropertyScopeInput)
	/// Output scope
	public static let output 		= PropertyScope(rawValue: kAudioObjectPropertyScopeOutput)
	/// Play through scope
	public static let playThrough 	= PropertyScope(rawValue: kAudioObjectPropertyScopePlayThrough)
	/// Wildcard scope
	public static let wildcard 		= PropertyScope(rawValue: kAudioObjectPropertyScopeWildcard)
}

/// A thin wrapper around a HAL audio object property element
public struct PropertyElement: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	public let rawValue: AudioObjectPropertyElement

	public init(rawValue: AudioObjectPropertyElement) {
		self.rawValue = rawValue
	}

	public init(integerLiteral value: UInt32) {
		self.rawValue = value
	}

	public init(stringLiteral value: StringLiteralType) {
		self.rawValue = value.fourCC
	}
}

extension PropertyElement {
	/// Master element
	public static let master 	= PropertyElement(rawValue: kAudioObjectPropertyElementMaster)
	/// Wildcard element
	public static let wildcard 	= PropertyElement(rawValue: kAudioObjectPropertyElementWildcard)
}

/// A thin wrapper around a HAL audio object property address
public struct PropertyAddress: RawRepresentable {
	public let rawValue: AudioObjectPropertyAddress

	public init(rawValue: AudioObjectPropertyAddress) {
		self.rawValue = rawValue
	}

	/// The property's selector
	public var selector: PropertySelector {
		return PropertySelector(rawValue: rawValue.mSelector)
	}

	/// The property's scope
	public var scope: PropertyScope {
		return PropertyScope(rawValue: rawValue.mScope)
	}

	/// The property's element
	public var element: PropertyElement {
		return PropertyElement(rawValue: rawValue.mElement)
	}

	/// Initializes a new `PropertyAddress` with the specified raw selector, scope, and element values
	/// - parameter selector: The desired raw selector value
	/// - parameter scope: The desired raw scope value
	/// - parameter element: The desired raw element value
	public init(_ selector: AudioObjectPropertySelector, scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal, element: AudioObjectPropertyElement = kAudioObjectPropertyElementMaster) {
		self.rawValue = AudioObjectPropertyAddress(mSelector: selector, mScope: scope, mElement: element)
	}

	/// Initializes a new `PropertyAddress` with the specified selector, scope, and element
	/// - parameter selector: The desired selector
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public init(_ selector: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) {
		self.rawValue = AudioObjectPropertyAddress(mSelector: selector.rawValue, mScope: scope.rawValue, mElement: element.rawValue)
	}
}

extension PropertyAddress: Hashable {
	public static func == (lhs: PropertyAddress, rhs: PropertyAddress) -> Bool {
		let l = lhs.rawValue
		let r = rhs.rawValue
		return l.mSelector == r.mSelector && l.mScope == r.mScope && l.mElement == r.mElement
		// Congruence?
//		return ((l.mSelector == r.mSelector) 	|| (l.mSelector == kAudioObjectPropertySelectorWildcard) 	|| (r.mSelector == kAudioObjectPropertySelectorWildcard))
//			&& ((l.mScope == r.mScope) 			|| (l.mScope == kAudioObjectPropertyScopeWildcard) 			|| (r.mScope == kAudioObjectPropertyScopeWildcard))
//			&& ((l.mElement == r.mElement) 		|| (l.mElement == kAudioObjectPropertyElementWildcard) 		|| (r.mElement == kAudioObjectPropertyElementWildcard))
	}

	public func hash(into hasher: inout Hasher) {
		hasher.combine(rawValue.mSelector)
		hasher.combine(rawValue.mScope)
		hasher.combine(rawValue.mElement)
	}
}

/// A HAL audio object property qualifier
public struct PropertyQualifier {
	/// The property qualifier's value
	public let value: UnsafeRawPointer
	/// The property qualifier's size
	public let size: UInt32

	/// Initializes a new `PropertyQualifier`with the specified value and size
	/// - parameter value: A pointer to the qualifier data
	/// - parameter size: The size in bytes of the data pointed to by `value`
	public init(value: UnsafeRawPointer, size: UInt32) {
		self.value = value
		self.size = size
	}

	/// Initializes a new `PropertyQualifier`with the specified value
	/// - parameter value: A pointer to the qualifier data
	public init<T>(_ value: UnsafePointer<T>) {
		self.value = UnsafeRawPointer(value)
		self.size = UInt32(MemoryLayout<T>.stride)
	}
}

// MARK: - Low-Level Property Support

/// Returns the size in bytes of `property` from `objectID`
/// - parameter property: The address of the desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func audioObjectPropertySize(_ property: PropertyAddress, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws -> Int {
	var propertyAddress = property.rawValue
	var dataSize: UInt32 = 0
	let result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}
	return Int(dataSize)
}

/// Reads `size` bytes of `property` from `objectID` into `ptr`
/// - parameter property: The address of the desired property
/// - parameter objectID: The audio object to query
/// - parameter ptr: A pointer to receive the property's value
/// - parameter size: The number of bytes to read
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func readAudioObjectProperty<T>(_ property: PropertyAddress, from objectID: AudioObjectID, into ptr: UnsafeMutablePointer<T>, size: Int = MemoryLayout<T>.stride, qualifier: PropertyQualifier? = nil) throws {
	var propertyAddress = property.rawValue
	var dataSize = UInt32(size)
	let result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize, ptr)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}
}

/// Writes `size` bytes from `ptr` to `property` on `objectID`
/// - parameter property: The address of the desired property
/// - parameter objectID: The audio object to change
/// - parameter ptr: A pointer to the desired property value
/// - parameter size: The number of bytes to write
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property, the property is not settable, or the property value could not be set
func writeAudioObjectProperty<T>(_ property: PropertyAddress, on objectID: AudioObjectID, from ptr: UnsafePointer<T>, size: Int = MemoryLayout<T>.stride, qualifier: PropertyQualifier? = nil) throws {
	var propertyAddress = property.rawValue
	let dataSize = UInt32(size)
	let result = AudioObjectSetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, dataSize, ptr)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectSetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}
}

// MARK: Numeric Property Helper

/// Returns the value of `property` from `objectID`
/// - parameter property: The address of the desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - parameter initialValue: An optional initial value for `outData` when calling `AudioObjectGetPropertyData`
/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
func getAudioObjectProperty<T: Numeric>(_ property: PropertyAddress, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil, initialValue: T = 0) throws -> T {
	var value = initialValue
	try readAudioObjectProperty(property, from: objectID, into: &value, qualifier: qualifier)
	return value
}

// MARK: - Array Properties

/// Returns the value of `property` from `objectID`
/// - parameter property: The address of the desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func getAudioObjectProperty<T>(_ property: PropertyAddress, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws -> [T] {
	let dataSize = try audioObjectPropertySize(property, from: objectID, qualifier: qualifier)
	let count = dataSize / MemoryLayout<T>.stride
	let array = try [T](unsafeUninitializedCapacity: count) { (buffer, initializedCount) in
		try readAudioObjectProperty(property, from: objectID, into: buffer.baseAddress!, size: dataSize, qualifier: qualifier)
		initializedCount = count
	}
	return array
}

/// Sets the value of `property` to `value` on `objectID`
/// - parameter property: The address of the desired property
/// - parameter value: The desired value
/// - parameter objectID: The audio object to change
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property, the property is not settable, or the property value could not be set
func setAudioObjectProperty<T>(_ property: PropertyAddress, to value: [T], on objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws {
	var data = value
	let dataSize = MemoryLayout<T>.stride * value.count
	try writeAudioObjectProperty(property, on: objectID, from: &data, size: dataSize, qualifier: qualifier)
}

// MARK: - Variable-Length Core Audio Structure Properties

/// Returns the value of `property` from `objectID`
/// - parameter property: The address of the desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func getAudioObjectProperty(_ property: PropertyAddress, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws -> AudioChannelLayoutWrapper {
	let dataSize = try audioObjectPropertySize(property, from: objectID, qualifier: qualifier)
	let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: dataSize)
	defer {
		mem.deallocate()
	}
	try readAudioObjectProperty(property, from: objectID, into: mem, size: dataSize, qualifier: qualifier)
	return AudioChannelLayoutWrapper(mem)
}

/// Returns the value of `property` from `objectID`
/// - parameter property: The address of the desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func getAudioObjectProperty(_ property: PropertyAddress, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws -> AudioBufferListWrapper {
	let dataSize = try audioObjectPropertySize(property, from: objectID, qualifier: qualifier)
	let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: dataSize)
	defer {
		mem.deallocate()
	}
	try readAudioObjectProperty(property, from: objectID, into: mem, size: dataSize, qualifier: qualifier)
	return AudioBufferListWrapper(mem)
}

// MARK: - Four Character Code Helpers

extension String {
	/// Returns `self.prefix(4)` interpreted as a four character code
	var fourCC: UInt32 {
		var fourcc: UInt32 = 0
		for uc in prefix(4).unicodeScalars {
			fourcc = (fourcc << 8) + (uc.value & 0xff)
		}
		return fourcc
	}

}

extension UInt32 {
	/// Returns the value of `self` interpreted as a four character code
	var fourCC: String {
		let chars: [UInt8] = [UInt8((self >> 24) & 0xff), UInt8((self >> 16) & 0xff), UInt8((self >> 8) & 0xff), UInt8(self & 0xff), 0]
		return String(cString: chars)
	}
}

// MARK: - Debugging Helpers

extension PropertySelector: CustomStringConvertible {
	public var description: String {
		return "'\(rawValue.fourCC)'"
	}
}

extension PropertyScope: CustomStringConvertible {
	public var description: String {
		return "'\(rawValue.fourCC)'"
	}
}

extension PropertyElement: CustomStringConvertible {
	public var description: String {
		return "\(rawValue)"
	}
}

extension PropertyAddress: CustomStringConvertible {
	public var description: String {
		"(\(selector.description), \(scope.description), \(element.description))"
	}
}

// MARK: - Variable-Length Core Audio Structure Wrappers

/// A thin wrapper around a variable-length `AudioChannelLayout` structure
public struct AudioChannelLayoutWrapper {
	/// The underlying memory
	let ptr: UnsafePointer<UInt8>

	/// Initializes a new `AudioChannelLayoutWrapper` wrapping `mem`
	init(_ mem: UnsafePointer<UInt8>) {
		ptr = mem
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
	public func withUnsafePointer<T>(_ block:(UnsafePointer<AudioChannelLayout>) throws -> T) rethrows -> T {
		return try ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { return try block($0) }
	}
}

import AVFoundation

extension AudioChannelLayoutWrapper {
	/// Returns `self` converted to an `AVAudioChannelLayout`object
	var avAudioChannelLayout: AVAudioChannelLayout {
		return withUnsafePointer { return AVAudioChannelLayout(layout: $0) }
	}
}

/// A thin wrapper around a variable-length `AudioBufferList` structure
public struct AudioBufferListWrapper {
	/// The underlying memory
	let ptr: UnsafePointer<UInt8>

	/// Initializes a new `AudioBufferListWrapper` wrapping `mem`
	init(_ mem: UnsafePointer<UInt8>) {
		ptr = mem
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
	public func withUnsafePointer<T>(_ block:(UnsafePointer<AudioBufferList>) throws -> T) rethrows -> T {
		return try ptr.withMemoryRebound(to: AudioBufferList.self, capacity: 1) { return try block($0) }
	}
}
