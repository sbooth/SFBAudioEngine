//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio
import os.log

/// A thin wrapper around a HAL audio object property selector
public struct PropertySelector: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	/// Wildcard selector
	public static let wildcard = PropertySelector(rawValue: kAudioObjectPropertySelectorWildcard)

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

/// A thin wrapper around a HAL audio object property scope
public struct PropertyScope: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	/// Global scope
	public static let global 		= PropertyScope(rawValue: kAudioObjectPropertyScopeGlobal)
	/// Input scope
	public static let input 		= PropertyScope(rawValue: kAudioObjectPropertyScopeInput)
	/// Output scope
	public static let output 		= PropertyScope(rawValue: kAudioObjectPropertyScopeOutput)
	/// Play through scope
	public static let playThrough 	= PropertyScope(rawValue: kAudioObjectPropertyScopePlayThrough)
	/// Wildcare scope
	public static let wildcard 		= PropertyScope(rawValue: kAudioObjectPropertyScopeWildcard)

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

/// A thin wrapper around a HAL audio object property element
public struct PropertyElement: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	/// Master element
	public static let master 	= PropertyElement(rawValue: kAudioObjectPropertyElementMaster)
	/// Wildcard element
	public static let wildcard 	= PropertyElement(rawValue: kAudioObjectPropertyElementWildcard)

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
	public init(_ selector: AudioObjectPropertySelector, in scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal, on element: AudioObjectPropertyElement = kAudioObjectPropertyElementMaster) {
		self.rawValue = AudioObjectPropertyAddress(mSelector: selector, mScope: scope, mElement: element)
	}

	/// Initializes a new `PropertyAddress` with the specified selector, scope, and element
	/// - parameter selector: The desired selector
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public init(_ selector: PropertySelector, in scope: PropertyScope = .global, on element: PropertyElement = .master) {
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

/// A HAL audio object property consisting of a property address and the associated underlying value type
public struct AudioObjectProperty<T> {
	/// The property's address
	public let address: PropertyAddress

	/// Initializes a new `AudioObjectProperty` with the specified address
	/// - parameter address: The desired address
	public init(_ address: PropertyAddress) {
		self.address = address
	}
}

extension AudioObjectProperty {
	/// Initializes a new `AudioObjectProperty` with the specified selector, scope, and element
	/// - parameter selector: The desired selector
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public init(_ selector: PropertySelector, in scope: PropertyScope = .global, on element: PropertyElement = .master) {
		self.address = PropertyAddress(selector, in: scope, on: element)
	}

	/// Initializes a new `AudioObjectProperty` with the specified raw selector, scope, and element values
	/// - parameter selector: The desired raw selector value
	/// - parameter scope: The desired raw scope value
	/// - parameter element: The desired raw element value
	public init(_ selector: AudioObjectPropertySelector, scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal, element: AudioObjectPropertyElement = kAudioObjectPropertyElementMaster) {
		self.address = PropertyAddress(selector, in: scope, on: element)
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

// MARK: - Property Retrieval

/// Returns the size in bytes of `property` from `objectID`
/// - parameter address: The desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func getPropertySize(_ property: PropertyAddress, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws -> Int {
	var propertyAddress = property.rawValue

	var dataSize: UInt32 = 0
	let result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}

	return Int(dataSize)
}

///// A fixed-length Core Audio C `struct` that can be stored and retrieved as an audio object property
//public protocol AudioObjectFixedLengthStructureProperty {}
//
//extension AudioValueRange: AudioObjectFixedLengthStructureProperty {}
//extension AudioStreamBasicDescription: AudioObjectFixedLengthStructureProperty {}
//extension AudioStreamRangedDescription: AudioObjectFixedLengthStructureProperty {}

/// Reads `size` bytes of `property` from `objectID` into `ptr`
/// - parameter property: The desired property
/// - parameter objectID: The audio object to query
/// - parameter ptr: A pointer to receive the propert's value
/// - parameter size: The number of bytes to read
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func readAudioObjectProperty<T>(_ property: AudioObjectProperty<T>, from objectID: AudioObjectID, into ptr: UnsafeMutablePointer<T>, size: Int = MemoryLayout<T>.stride, qualifier: PropertyQualifier? = nil) throws {
	var propertyAddress = property.address.rawValue

	var dataSize = UInt32(size)
	let result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize, ptr)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}
}

// Helper for the common case with numeric properties

/// Returns the value of `property` from `objectID`
/// - parameter property: The desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - parameter initialValue: An optional initial value for `outData` when calling `AudioObjectGetPropertyData`
/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
func getAudioObjectProperty<T: Numeric>(_ property: AudioObjectProperty<T>, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil, initialValue: T = 0) throws -> T {
	var value = initialValue
	try readAudioObjectProperty(property, from: objectID, into: &value, qualifier: qualifier)
	return value
}

// MARK: - Scalar Property Setting

/// Sets the value of `property` to `value` on `objectID`
/// - parameter property: The desired property
/// - parameter value: The desired value
/// - parameter objectID: The audio object to change
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property, the property is not settable, or the property value could not be set
func setAudioObjectProperty<T>(_ property: AudioObjectProperty<T>, to value: T, on objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws {
	var propertyAddress = property.address.rawValue

	var data = value
	let dataSize = UInt32(MemoryLayout<T>.stride)
	let result = AudioObjectSetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, dataSize, &data)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectSetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}
}

// MARK: - Array Properties

/// Returns the value of `property` from `objectID`
/// - parameter property: The desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func getAudioObjectProperty<T>(_ property: AudioObjectProperty<[T]>, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws -> [T] {
	var propertyAddress = property.address.rawValue

	var dataSize: UInt32 = 0
	var result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}

	let count = Int(dataSize) / MemoryLayout<T>.stride

	let array = try [T](unsafeUninitializedCapacity: count) { (buffer, initializedCount) in
		result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize, UnsafeMutableRawPointer(buffer.baseAddress!))
		guard result == kAudioHardwareNoError else {
			os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
			throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
		}
		initializedCount = count
	}

	return array

//	let data = UnsafeMutablePointer<T>.allocate(capacity: count)
//	defer {
//		data.deallocate()
//	}

//	result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize, data)
//	guard result == kAudioHardwareNoError else {
//		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
//		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
//	}

//	return [T](UnsafeBufferPointer(start: data, count: count))
}

/// Sets the value of `property` to `value` on `objectID`
/// - parameter property: The desired property
/// - parameter value: The desired value
/// - parameter objectID: The audio object to change
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property, the property is not settable, or the property value could not be set
func setAudioObjectProperty<T>(_ property: AudioObjectProperty<[T]>, to value: [T], on objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws {
	var propertyAddress = property.address.rawValue

	var data = value
	let dataSize = UInt32(MemoryLayout<T>.stride * value.count)
	let result = AudioObjectSetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, dataSize, &data)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectSetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}
}

// MARK: - Variable-Length Core Audio Structure Properties

///// A variable-length Core Audio C `struct` that can be stored and retrieved as an audio object property
//public protocol AudioObjectVariableLengthStructureProperty {}
//
//extension AudioChannelLayout: AudioObjectVariableLengthStructureProperty {}
//extension AudioBufferList: AudioObjectVariableLengthStructureProperty {}

/// Returns the value of `property` from `objectID`
/// - parameter property: The desired property
/// - parameter objectID: The audio object to query
/// - parameter qualifier: An optional property qualifier
/// - throws: An exception if the object does not have the requested property or the property value could not be retrieved
func getAudioObjectProperty(_ property: AudioObjectProperty<AudioChannelLayout>, from objectID: AudioObjectID, qualifier: PropertyQualifier? = nil) throws -> AudioChannelLayoutWrapper {
	var propertyAddress = property.address.rawValue

	var dataSize: UInt32 = 0
	var result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}

	let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: Int(dataSize))
	defer {
		mem.deallocate()
	}

	result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier?.size ?? 0, qualifier?.value, &dataSize, mem)
	guard result == kAudioHardwareNoError else {
		os_log(.error, log: audioObjectLog, "AudioObjectGetPropertyData (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
		mem.deallocate()
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
	}

//	return ManagedAudioChannelLayout(audioChannelLayoutPointer: AudioChannelLayout.UnsafePointer(data)) { $0.unsafePointer.deallocate()	}
	return AudioChannelLayoutWrapper(mem)
//	return AudioChannelLayout.UnsafePointer(UnsafeRawPointer(mem).assumingMemoryBound(to: AudioChannelLayout.self))
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

extension AudioObjectProperty: CustomStringConvertible {
	public var description: String {
		"\(address) → \(T.self)"
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
	public var channelDescriptions: UnsafeBufferPointer<AudioChannelDescription>? {
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
