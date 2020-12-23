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
	public func propertyIsSettable(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		return try __propertyIsSettable(property, in: scope, onElement: element).boolValue
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

	/// Returns the value for `property` as an `UInt32`
	/// - note: `property` must refer to a property of type `UInt32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt32 {
		return try __unsignedInt(forProperty: property, in: scope, onElement: element).uint32Value
	}

	/// Returns the value for `property` as an array `UInt32`
	/// - note: `property` must refer to a property of type array of `UInt32`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [UInt32] {
		return try __unsignedIntArray(forProperty: property, in: scope, onElement: element).map { $0.uint32Value }
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
		return try __string(forProperty: property, in: scope, onElement: element, qualifier: nil, qualifierSize: 0)
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

	/// Returns the value for `property` as a `Array`
	/// - note: `property` must refer to a property of type `CFArrayRef`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [Any] {
		return try __array(forProperty: property, in: scope, onElement: element)
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
		return try __forProperty(property, in: scope, onElement: element, qualifier: nil, qualifierSize: 0)
	}

	/// Returns the value for `property` as an array of `AudioObject`
	/// - note: `property` must refer to a property of type array of `AudioObjectID`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioObject] {
		return try __audioObjectArray(forProperty: property, in: scope, onElement: element, qualifier: nil, qualifierSize: 0)
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

	/// Returns the value for `property` as a wrapped `AudioChannelLayout` structure
	/// - note: `property` must refer to a property of type `AudioChannelLayout`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioChannelLayoutWrapper {
		return try __audioChannelLayout(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as a wrapped `AudioBufferList` structure
	/// - note: `property` must refer to a property of type `AudioBufferList`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioBufferListWrapper {
		return try __audioBufferList(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as a wrapped `AudioHardwareIOProcStreamUsageWrapper` structure
	/// - note: `property` must refer to a property of `AudioHardwareIOProcStreamUsage`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioHardwareIOProcStreamUsageWrapper {
		return try __audioHardwareIOProcStreamUsage(forProperty: property, in: scope, onElement: element)
	}

	/// Returns the value for `property` as a `WorkGroup`
	/// - note: `property` must refer to a property of type array of `os_workgroup_t`
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The property value
	/// - throws: An error if the property could not be retrieved
	@available(macOS 11.0, *)
	public func getProperty(_ property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> WorkGroup {
		return try __osWorkgroup(forProperty: property, in: scope, onElement: element)
	}

}

extension AudioObject {

	// MARK: - Property Setting

	/// Sets the value for `property` as a `UInt32`
	/// - note: `property` must refer to a property of type `UInt32`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: UInt32, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setUnsignedInt(value, forProperty: property, in: scope, onElement: element)
	}

	/// Sets the value for `property` as an array of `UInt32`
	/// - note: `property` must refer to a property of type array of `UInt32`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: [UInt32], scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setUnsignedIntArray(value.map { NSNumber(value: $0) }, forProperty: property, in: scope, onElement: element)
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

	/// Sets the value for `property` as an `AudioStreamBasicDescription`
	/// - note: `property` must refer to a property of type `AudioStreamBasicDescription`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: AudioStreamBasicDescription, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setAudioStreamBasicDescription(value, forProperty: property, in: scope, onElement: element)
	}

	/// Sets the value for `property` as an `AudioObject`
	/// - note: `property` must refer to a property of type `AudioObjectID`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: AudioObject, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setAudioObject(value, forProperty: property, in: scope, onElement: element)
	}

	/// Sets the value for `property` as an `AudioChannelLayoutWrapper`
	/// - note: `property` must refer to a property of type `AudioChannelLayout`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: AudioChannelLayoutWrapper, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setAudioChannelLayout(value, forProperty: property, in: scope, onElement: element)
	}

	/// Sets the value for `property` as an `AudioBufferListWrapper`
	/// - note: `property` must refer to a property of type `AudioBufferList`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: AudioBufferListWrapper, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setAudioBufferList(value, forProperty: property, in: scope, onElement: element)
	}

	/// Sets the value for `property` as an `AudioHardwareIOProcStreamUsageWrapper`
	/// - note: `property` must refer to a property of type `AudioBufferList`
	/// - parameter property: The property to set
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setProperty(_ property: PropertySelector, _ value: AudioHardwareIOProcStreamUsageWrapper, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try __setAudioHardwareIOProcStreamUsage(value, forProperty: property, in: scope, onElement: element)
	}
}

extension AudioObject {
	/// Returns the audio object's base class
	/// - note: This corresponds to `kAudioObjectPropertyBaseClass`
	/// - returns: The audio object's base class
	/// - throws: An error if the property could not be retrieved
	public func baseClassID() throws -> AudioClassID {
		return AudioClassID(try getProperty(.baseClass) as UInt32)
	}

	/// Returns the audio object's class
	/// - note: This corresponds to `kAudioObjectPropertyClass`
	/// - returns: The audio object's class
	/// - throws: An error if the property could not be retrieved
	public func classID() throws -> AudioClassID {
		return AudioClassID(try getProperty(.class) as UInt32)
	}

	/// Returns the audio object's owning object
	/// - note: This corresponds to `kAudioObjectPropertyOwner`
	/// - note: The system object does not have an owner
	/// - returns: The audio object's owning object
	/// - throws: An error if the property could not be retrieved
	public func owner() throws -> AudioObject? {
		return try getProperty(.owner)
	}

	/// Returns the audio object's name
	/// - note: This corresponds to `kAudioObjectPropertyName`
	/// - returns: The audio object's name
	/// - throws: An error if the property could not be retrieved
	public func name() throws -> String {
		return try getProperty(.name)
	}

	/// Returns the audio object's model name
	/// - note: This corresponds to `kAudioObjectPropertyModelName`
	/// - returns: The audio object's model name
	/// - throws: An error if the property could not be retrieved
	public func modelName() throws -> String {
		return try getProperty(.modelName)
	}

	/// Returns the audio object's manufacturer
	/// - note: This corresponds to `kAudioObjectPropertyManufacturer`
	/// - returns: The audio object's manufacturer
	/// - throws: An error if the property could not be retrieved
	public func manufacturer() throws -> String {
		return try getProperty(.manufacturer)
	}

	/// Returns the name of the specified element
	/// - note: This corresponds to `kAudioObjectPropertyElementName`
	/// - parameter element: The desired element
	/// - parameter scope: The desired scope
	/// - returns: The name of the specified element
	/// - throws: An error if the property could not be retrieved
	public func nameOfElement(_ element: PropertyElement, scope: PropertyScope = .global) throws -> String {
		return try getProperty(.elementName, scope: scope, element: element)
	}

	/// Returns the category name of the specified element
	/// - note: This corresponds to `kAudioObjectPropertyElementCategoryName`
	/// - parameter element: The desired element
	/// - parameter scope: The desired scope
	/// - returns: The category name of the specified element
	/// - throws: An error if the property could not be retrieved
	public func categoryNameOfElement(_ element: PropertyElement, scope: PropertyScope = .global) throws -> String {
		return try getProperty(.elementName, scope: scope, element: element)
	}

	/// Returns the number name of the specified element
	/// - note: This corresponds to `kAudioObjectPropertyElementNumberName`
	/// - returns: The drift compensation
	/// - throws: An error if the property could not be retrieved
	public func numberNameOfElement(_ element: PropertyElement, scope: PropertyScope = .global) throws -> String {
		return try getProperty(.elementName, scope: scope, element: element)
	}

	/// Returns the audio objects owned by this object
	/// - note: This corresponds to `kAudioObjectPropertyOwnedObjects`
	/// - returns: The audio objects owned by this object
	/// - throws: An error if the property could not be retrieved
	public func ownedObjects() throws -> [AudioObject] {
		return try getProperty(.ownedObjects)
	}

	/// Returns the audio objects owned by this object
	/// - note: This corresponds to `kAudioObjectPropertyOwnedObjects`
	/// - returns: The audio objects owned by this object
	/// - throws: An error if the property could not be retrieved
	public func ownedObjects(types: [UInt32]) throws -> [AudioObject] {
		let qualifier: [AudioClassID] = types.map { AudioClassID($0)  }
		let qualifierSize: UInt32 = UInt32(MemoryLayout<AudioClassID>.size * types.count)
		return try __audioObjectArray(forProperty: .ownedObjects, in: .global, onElement: .master, qualifier: qualifier, qualifierSize: qualifierSize)
	}

	/// Returns the audio object's serial number
	/// - note: This corresponds to `kAudioObjectPropertySerialNumber`
	/// - returns: The audio object's serial number
	/// - throws: An error if the property could not be retrieved
	public func serialNumber() throws -> String {
		return try getProperty(.serialNumber)
	}

	/// Returns the audio object's firmware version
	/// - note: This corresponds to `kAudioObjectPropertyFirmwareVersion`
	/// - returns: The audio object's firmware version
	/// - throws: An error if the property could not be retrieved
	public func firmwareVersion() throws -> String {
		return try getProperty(.firmwareVersion)
	}
}

extension AudioObject {
	/// Translates `value` using an `AudioValueTranslation` structure and returns the translated value
	/// - note: `property` must accept an `AudioValueTranslation` structure having `UInt32` for input and `CFStringRef` for output
	/// - parameter value: The value to translate
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The translated value
	/// - throws: An error if the value could not be translated
	func translateValue(_ value: UInt32, using property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> String {
		return try __translateToString(fromUnsignedInteger: value, usingProperty: property, in: scope, onElement: element)
	}

	/// Translates `value` using an `AudioValueTranslation` structure and returns the translated value
	/// - note: `property` must accept an `AudioValueTranslation` structure having `UInt32` for input and `UInt32` for output
	/// - parameter value: The value to translate
	/// - parameter property: The property to query
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The translated value
	/// - throws: An error if the value could not be translated
	func translateValue(_ value: UInt32, using property: PropertySelector, scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt32 {
		return try __translateToUnsignedInteger(fromUnsignedInteger: value, usingProperty: property, in: scope, onElement: element).uint32Value
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

extension AudioBufferListWrapper {
	/// Returns the buffer list's `mBuffers`
	public var buffers: UnsafeBufferPointer<AudioBuffer> {
		return UnsafeBufferPointer(start: __buffers, count: Int(audioBufferList.pointee.mNumberBuffers))
	}
}

extension AudioChannelLayoutWrapper {
	/// Returns the layout's `mChannelDescriptions`
	public var channelDescriptions: UnsafeBufferPointer<AudioChannelDescription> {
		return UnsafeBufferPointer(start: __channelDescriptions, count: Int(audioChannelLayout.pointee.mNumberChannelDescriptions))
	}
}

extension AudioHardwareIOProcStreamUsageWrapper {
	/// Returns `mStreamIsOn`
	public var streamIsOn: UnsafeBufferPointer<UInt32> {
		return UnsafeBufferPointer(start: __streamIsOn, count: Int(audioHardwareIOProcStreamUsage.pointee.mNumberStreams))
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
