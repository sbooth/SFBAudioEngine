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

	/// Creates a new instance with the specified value
	/// - parameter value: The value to use for the new instance
	public init(_ value: AudioObjectPropertySelector) {
		self.rawValue = value
	}

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
	public static let wildcard = PropertySelector(kAudioObjectPropertySelectorWildcard)
}

/// A thin wrapper around a HAL audio object property scope
public struct PropertyScope: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	public let rawValue: AudioObjectPropertyScope

	/// Creates a new instance with the specified value
	/// - parameter value: The value to use for the new instance
	public init(_ value: AudioObjectPropertyScope) {
		self.rawValue = value
	}

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
	public static let global 		= PropertyScope(kAudioObjectPropertyScopeGlobal)
	/// Input scope
	public static let input 		= PropertyScope(kAudioObjectPropertyScopeInput)
	/// Output scope
	public static let output 		= PropertyScope(kAudioObjectPropertyScopeOutput)
	/// Play-through scope
	public static let playThrough 	= PropertyScope(kAudioObjectPropertyScopePlayThrough)
	/// Wildcard scope
	public static let wildcard 		= PropertyScope(kAudioObjectPropertyScopeWildcard)
}

/// A thin wrapper around a HAL audio object property element
public struct PropertyElement: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
	public let rawValue: AudioObjectPropertyElement

	/// Creates a new instance with the specified value
	/// - parameter value: The value to use for the new instance
	public init(_ value: AudioObjectPropertyElement) {
		self.rawValue = value
	}

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
	public static let master 	= PropertyElement(kAudioObjectPropertyElementMaster)
	/// Wildcard element
	public static let wildcard 	= PropertyElement(kAudioObjectPropertyElementWildcard)
}

/// A thin wrapper around a HAL audio object property address
public struct PropertyAddress: RawRepresentable {
	public let rawValue: AudioObjectPropertyAddress

	/// Creates a new instance with the specified value
	/// - parameter value: The value to use for the new instance
	public init(_ value: AudioObjectPropertyAddress) {
		self.rawValue = value
	}

	public init(rawValue: AudioObjectPropertyAddress) {
		self.rawValue = rawValue
	}

	/// The property's selector
	public var selector: PropertySelector {
		return PropertySelector(rawValue.mSelector)
	}

	/// The property's scope
	public var scope: PropertyScope {
		return PropertyScope(rawValue.mScope)
	}

	/// The property's element
	public var element: PropertyElement {
		return PropertyElement(rawValue.mElement)
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

	/// Creates a new instance with the specified value and size
	/// - parameter value: A pointer to the qualifier data
	/// - parameter size: The size in bytes of the data pointed to by `value`
	public init(value: UnsafeRawPointer, size: UInt32) {
		self.value = value
		self.size = size
	}

	/// Creates a new instance with the specified value
	///
	/// `size` is initlalized to `MemoryLayout<T>.stride`
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
		let userInfo = [NSLocalizedDescriptionKey: NSLocalizedString("Size information for the property \(property.selector) in scope \(property.scope) on audio object 0x\(String(objectID, radix: 16, uppercase: false)) could not be retrieved.", comment: "")]
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: userInfo)
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
		let userInfo = [NSLocalizedDescriptionKey: NSLocalizedString("The property \(property.selector) in scope \(property.scope) on audio object 0x\(String(objectID, radix: 16, uppercase: false)) could not be retrieved.", comment: "")]
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: userInfo)
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
		let userInfo = [NSLocalizedDescriptionKey: NSLocalizedString("The property \(property.selector) in scope \(property.scope) on audio object 0x\(String(objectID, radix: 16, uppercase: false)) could not be set.", comment: "")]
		throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: userInfo)
	}
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
		return rawValue == kAudioObjectPropertyElementMaster ? "master" : "\(rawValue)"
	}
}

extension PropertyAddress: CustomStringConvertible {
	public var description: String {
		"(\(selector.description), \(scope.description), \(element.description))"
	}
}
