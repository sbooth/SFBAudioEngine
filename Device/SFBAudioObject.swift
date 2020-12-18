/*
* Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioObject {

	// MARK: - Property Information

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

	// MARK: - Property Observation

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

extension AudioObject {

	// MARK: - Property Retrieval

	/// Returns the value for `property` as an `UInt`
	/// - note: `property` must refer to a property of type `UInt32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt {
		return try __unsignedInt(forProperty: property, in: scope, onElement: element).uintValue
	}

	/// Returns the value for `property` as an array `UInt`
	/// - note: `property` must refer to a property of type array of `UInt32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [UInt] {
		return try __unsignedIntArray(forProperty: property, in: scope, onElement: element).map { $0.uintValue }
	}

	/// Returns the value for `property` as a `Float`
	/// - note: `property` must refer to a property of type `Float32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Float {
		return try __float(forProperty: property, in: scope, onElement: element).floatValue
	}

	/// Returns the value for `property` as a `Double`
	/// - note: `property` must refer to a property of type `Float64`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Double {
		return try __double(forProperty: property, in: scope, onElement: element).doubleValue
	}

	/// Returns the value for `property` as a `String`
	/// - note: `property` must refer to a property of type `CFStringRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> String {
		return try __string(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as a `Dictionary`
	/// - note: `property` must refer to a property of type `CFDictionaryRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AnyHashable: Any] {
		return try __dictionary(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as a `URL`
	/// - note: `property` must refer to a property of type `CFURLRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> URL {
		return try __url(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as an `AudioObject`
	/// - note: `property` must refer to a property of type `AudioObjectID`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioObject {
		return try __forProperty(property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as an array of `AudioObject`
	/// - note: `property` must refer to a property of type array of `AudioObjectID`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioObject] {
		return try __audioObjectArray(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as an `AudioStreamBasicDescription`
	/// - note: `property` must refer to a property of type `AudioStreamBasicDescription`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioStreamBasicDescription {
		return try __audioStreamBasicDescription(forProperty: property, in: scope, onElement: element).audioStreamBasicDescriptionValue()
	}

	/// Returns the value for `property` as an `AudioValueRange`
	/// - note: `property` must refer to a property of type `AudioValueRange`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioValueRange {
		return try __audioValueRange(forProperty: property, in: scope, onElement: element).audioValueRangeValue()
	}

	/// Returns the value for `property` as an array of `AudioStreamRangedDescription`
	/// - note: `property` must refer to a property of type array of `AudioStreamRangedDescription`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioStreamRangedDescription] {
		return try __audioStreamRangedDescriptionArray(forProperty: property, in: scope, onElement: element).map { $0.audioStreamRangedDescriptionValue() }
	}

	/// Returns the value for `property` as an array of `AudioValueRange`
	/// - note: `property` must refer to a property of type array of `AudioValueRange`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioValueRange] {
		return try __audioValueRangeArray(forProperty: property, in: scope, onElement: element).map { $0.audioValueRangeValue() }
	}

}

extension AudioObject {

	// MARK: - Property Setting

	/// Sets the value for `property` as a `UInt`
	/// - note: `property` must refer to a property of type `UInt32`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: UInt, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setUnsignedInt(UInt32(value), forProperty: property, in: scope, onElement: element)
	}

	/// Sets the value for `property` as a `Float`
	/// - note: `property` must refer to a property of type `Float32`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: Float, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setFloat(value, forProperty: property, in: scope, onElement: element)
	}

	/// Sets the value for `property` as a `Double`
	/// - note: `property` must refer to a property of type `Float64`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: Double, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setDouble(value, forProperty: property, in: scope, onElement: element)
	}

}

extension AudioObject {
	/// Returns the audio object's base class
	/// - note: This corresponds to `kAudioObjectPropertyBaseClass`
	func baseClassID() throws -> AudioClassID {
		return AudioClassID(try getProperty(.baseClass) as UInt)
	}

	/// Returns the audio object's class
	/// - note: This corresponds to `kAudioObjectPropertyClass`
	func classID() throws -> AudioClassID {
		return AudioClassID(try getProperty(.class) as UInt)
	}

	/// Returns the audio object's owning object
	/// - note: This corresponds to `kAudioObjectPropertyOwner`
	/// - note: The system object does not have an owner
	func owner() throws -> AudioObject {
		return try getProperty(.owner)
	}

	/// Returns the audio object's name
	/// - note: This corresponds to `kAudioObjectPropertyName`
	func name() throws -> String {
		return try getProperty(.name)
	}

	/// Returns the audio object's model name
	/// - note: This corresponds to `kAudioObjectPropertyModelName`
	func modelName() throws -> String {
		return try getProperty(.modelName)
	}

	/// Returns the audio object's manufacturer
	/// - note: This corresponds to `kAudioObjectPropertyManufacturer`
	func manufacturer() throws -> String {
		return try getProperty(.manufacturer)
	}

	/// Returns the name of the specified element
	/// - note: This corresponds to `kAudioObjectPropertyElementName`
	func nameOfElement(_ element: PropertyElement, scope: PropertyScope = .global) throws -> String {
		return try getProperty(.elementName, scope: scope, element: element)
	}

	/// Returns the category name of the specified element
	/// - note: This corresponds to `kAudioObjectPropertyElementCategoryName`
	func categoryNameOfElement(_ element: PropertyElement, scope: PropertyScope = .global) throws -> String {
		return try getProperty(.elementName, scope: scope, element: element)
	}

	/// Returns the number name of the specified element
	/// - note: This corresponds to `kAudioObjectPropertyElementNumberName`
	func numberNameOfElement(_ element: PropertyElement, scope: PropertyScope = .global) throws -> String {
		return try getProperty(.elementName, scope: scope, element: element)
	}

	/// Returns the audio objects owned by this object
	/// - note: This corresponds to `kAudioObjectPropertyOwnedObjects`
	func ownedObjects() throws -> [AudioObject] {
		return try getProperty(.ownedObjects)
	}

	/// Returns the audio object's serial number
	/// - note: This corresponds to `kAudioObjectPropertySerialNumber`
	func serialNumber() throws -> String {
		return try getProperty(.serialNumber)
	}

	/// Returns the audio object's firmware version
	/// - note: This corresponds to `kAudioObjectPropertyFirmwareVersion`
	func firmwareVersion() throws -> String {
		return try getProperty(.firmwareVersion)
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
