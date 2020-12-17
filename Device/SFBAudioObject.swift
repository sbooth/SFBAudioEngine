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
	public func uintForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> UInt? {
		guard let value = __uint(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value.uintValue
	}

	/// Returns the value for `property` as an array `UInt` or `nil` on error
	/// - note: `property` must refer to a property of type array of `UInt32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func uintsForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> [UInt]? {
		guard let value = __uintArray(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value.map { $0.uintValue }
	}

	/// Returns the value for `property` as a `Float` or `nil` on error
	/// - note: `property` must refer to a property of type `Float32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func floatForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> Float? {
		guard let value = __float(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value.floatValue
	}

	/// Returns the value for `property` as a `Double` or `nil` on error
	/// - note: `property` must refer to a property of type `Float64`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func doubleForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> Double? {
		guard let value = __double(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value.doubleValue
	}

	/// Returns the value for `property` as a `String` or `nil` on error
	/// - note: `property` must refer to a property of type `CFStringRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func stringForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> String? {
		guard let value = __string(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value
	}

	/// Returns the value for `property` as a `Dictionary` or `nil` on error
	/// - note: `property` must refer to a property of type `CFDictionaryRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func dictionaryForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> [AnyHashable: Any]? {
		guard let value = __dictionary(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value
	}

	/// Returns the value for `property` as an `AudioObject` or `nil` on error
	/// - note: `property` must refer to a property of type `AudioObjectID`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func audioObjectForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> AudioObject? {
		guard let value = __forProperty(property, in: scope, onElement: element) else {
			return nil
		}
		return value
	}

	/// Returns the value for `property` as an array of `AudioObject` or `nil` on error
	/// - note: `property` must refer to a property of type array of `AudioObjectID`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func audioObjectArrayForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> [AudioObject]? {
		guard let value = __audioObjectArray(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value
	}

	/// Returns the value for `property` as an `AudioStreamBasicDescription` or `nil` on error
	/// - note: `property` must refer to a property of type `AudioStreamBasicDescription`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func audioStreamBasicDescriptionForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> AudioStreamBasicDescription? {
		guard let value = __audioStreamBasicDescription(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return value.audioStreamBasicDescriptionValue()
	}

	/// Returns the value for `property` as an array of `AudioStreamRangedDescription` or `nil` on error
	/// - note: `property` must refer to a property of type array of `AudioStreamRangedDescription`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func audioStreamRangedDescriptionArrayForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> [AudioStreamRangedDescription]? {
		guard let values = __audioStreamRangedDescriptionArray(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return values.map { $0.audioStreamRangedDescriptionValue() }
	}

	/// Returns the value for `property` as an array of `AudioValueRange` or `nil` on error
	/// - note: `property` must refer to a property of type array of `AudioValueRange`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	public func audioValueRangeArrayForProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) -> [AudioValueRange]? {
		guard let values = __audioValueRangeArray(forProperty: property, in: scope, onElement: element) else {
			return nil
		}
		return values.map { $0.audioValueRangeValue() }
	}

	/// Performs `block` when the specified property changes
	/// - parameter property: The property to observe
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	public func whenPropertyChanges(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master, perform block:(() -> Void)?) {
		__whenProperty(property, in: scope, changesOnElement: element, perform: block)
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
		@unknown default: 		return "UNKNOWN"
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
		@unknown default: 		return "UNKNOWN"
		}
	}
}
