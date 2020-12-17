/*
* Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioObject {

	/// Returns `true` if the underlying audio object has the specified property
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: `true` if the property is supported
	public func hasProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> Bool {
		return __hasProperty(property, in: scope, onElement: element)
	}

	/// Returns `true` if the underlying audio object has the specified property and it is settable
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: `true` if the property is settable
	public func propertyIsSettable(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> Bool {
		return __propertyIsSettable(property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as an `UInt` or `nil` on error
	/// - note: `property` must refer to a property of type `UInt32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func uintForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt {
		return try __uint(forProperty: property, in: scope, onElement: element).uintValue
	}

	/// Returns the value for `property` as an array `UInt` or `nil` on error
	/// - note: `property` must refer to a property of type array of `UInt32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func uintsForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [UInt] {
		return try __uintArray(forProperty: property, in: scope, onElement: element).map { $0.uintValue }
	}

	/// Returns the value for `property` as a `Float` or `nil` on error
	/// - note: `property` must refer to a property of type `Float32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func floatForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Float {
		return try __float(forProperty: property, in: scope, onElement: element).floatValue
	}

	/// Returns the value for `property` as a `Double` or `nil` on error
	/// - note: `property` must refer to a property of type `Float64`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func doubleForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Double {
		return try __double(forProperty: property, in: scope, onElement: element).doubleValue
	}

	/// Returns the value for `property` as a `String` or `nil` on error
	/// - note: `property` must refer to a property of type `CFStringRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func stringForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> String {
		return try __string(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as a `Dictionary` or `nil` on error
	/// - note: `property` must refer to a property of type `CFDictionaryRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func dictionaryForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AnyHashable: Any] {
		return try __dictionary(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as an `AudioObject` or `nil` on error
	/// - note: `property` must refer to a property of type `AudioObjectID`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func audioObjectForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioObject {
		return try __forProperty(property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as an array of `AudioObject` or `nil` on error
	/// - note: `property` must refer to a property of type array of `AudioObjectID`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func audioObjectsForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioObject] {
		return try __audioObjectArray(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as an `AudioStreamBasicDescription` or `nil` on error
	/// - note: `property` must refer to a property of type `AudioStreamBasicDescription`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func audioStreamBasicDescriptionForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioStreamBasicDescription {
		return try __audioStreamBasicDescription(forProperty: property, in: scope, onElement: element).audioStreamBasicDescriptionValue()
	}

	/// Returns the value for `property` as an `AudioValueRange` or `nil` on error
	/// - note: `property` must refer to a property of type `AudioValueRange`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func audioValueRangeForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioValueRange {
		return try __audioValueRange(forProperty: property, in: scope, onElement: element).audioValueRangeValue()
	}

	/// Returns the value for `property` as an array of `AudioStreamRangedDescription` or `nil` on error
	/// - note: `property` must refer to a property of type array of `AudioStreamRangedDescription`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func audioStreamRangedDescriptionsForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioStreamRangedDescription] {
		return try __audioStreamRangedDescriptionArray(forProperty: property, in: scope, onElement: element).map { $0.audioStreamRangedDescriptionValue() }
	}

	/// Returns the value for `property` as an array of `AudioValueRange` or `nil` on error
	/// - note: `property` must refer to a property of type array of `AudioValueRange`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func audioValueRangesForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioValueRange] {
		return try __audioValueRangeArray(forProperty: property, in: scope, onElement: element).map { $0.audioValueRangeValue() }
	}

	/// Performs `block` when the specified property changes
	/// - parameter property: The property to observe
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be set
	public func whenPropertyChanges(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master, perform block:(() -> Void)?) throws {
		try __whenProperty(property, in: scope, changesOnElement: element, perform: block)
	}

}

extension AudioObject.PropertySelector: ExpressibleByStringLiteral {
	public init(stringLiteral value: StringLiteralType) {
		var fourcc: UInt32 = 0
		for uc in value.prefix(4).unicodeScalars {
			fourcc = (fourcc << 8) + (uc.value & 0xff)
		}
		self.init(rawValue: fourcc)!
	}
}

extension AudioObject.PropertyElement {
	/// The master element, `kAudioObjectPropertyElementMaster`
	public static var master: AudioObject.PropertyElement {
		return kAudioObjectPropertyElementMaster
	}

	/// The wildcard element, `kAudioObjectPropertyElementWildcard`
	public static var wildcard: AudioObject.PropertyElement {
		return kAudioObjectPropertyElementWildcard
	}
}

// MARK: - Debugging Helpers

extension AudioObject.PropertySelector: CustomDebugStringConvertible {
	public var debugDescription: String {
		let bytes: [CChar] = [
			0x27, // '
			CChar((rawValue >> 24) & 0xff),
			CChar((rawValue >> 16) & 0xff),
			CChar((rawValue >> 8) & 0xff),
			CChar(rawValue & 0xff),
			0x27, // '
			0
		]
		return String(cString: bytes)
	}
}

extension AudioObject.PropertyScope: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .global: 			return ".global"
		case .input: 			return ".input"
		case .output: 			return ".output"
		case .playThrough: 		return ".playThrough"
		case .wildcard: 		return ".wildcard"
		@unknown default: 		return "UNKNOWN (\(self.rawValue))"
		}
	}
}

extension AudioObject.PropertyElement: CustomDebugStringConvertible {
	public var debugDescription: String {
		if self == kAudioObjectPropertyElementMaster {
			return ".master"
		}
		else if self == kAudioObjectPropertyElementWildcard {
			return ".wildcard"
		}
		else {
			return "\(self)"
		}
	}
}

extension AudioDevice.TransportType: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .unknown:			return ".unknown"
		case .builtIn:			return ".builtIn"
		case .aggregate:		return ".aggregate"
		case .virtual:			return ".virtual"
		case .PCI:				return ".PCI"
		case .USB:				return ".USB"
		case .fireWire:			return ".fireWire"
		case .bluetooth:		return ".bluetooth"
		case .bluetoothLE:		return ".bluetoothLE"
		case .HDMI:				return ".HDMI"
		case .displayPort:		return ".displayPort"
		case .airPlay:			return ".airPlay"
		case .AVB:				return ".AVB"
		case .thunderbolt:		return ".thunderbolt"
		@unknown default: 		return "UNKNOWN (\(self.rawValue))"
		}
	}
}
