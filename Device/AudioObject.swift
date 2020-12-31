//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio
import os.log

/// A HAL audio object
public class AudioObject {
	/// The underlying audio object ID
	public let objectID: AudioObjectID

	/// Initializes an `AudioObject` with `objectID`
	/// - parameter objectID: The HAL audio object ID
	init(_ objectID: AudioObjectID) {
		precondition(objectID != kAudioObjectUnknown)
//		if objectID == kAudioObjectUnknown {
//			return nil
//		}
		self.objectID = objectID
	}

	/// Registered audio object property listeners
	private var listenerBlocks = [PropertyAddress: AudioObjectPropertyListenerBlock]()

	deinit {
		for (property, listenerBlock) in listenerBlocks {
			var propertyAddress = property.rawValue
			let result = AudioObjectRemovePropertyListenerBlock(objectID, &propertyAddress, DispatchQueue.global(qos: .background), listenerBlock)
			if result != kAudioHardwareNoError {
				os_log(.error, log: audioObjectLog, "AudioObjectRemovePropertyListenerBlock (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
			}
		}
	}

	/// Returns `true` if `self` has `property`
	/// - parameter property: The property to query
	public final func hasProperty(_ property: PropertyAddress) -> Bool {
		var address = property.rawValue
		return AudioObjectHasProperty(objectID, &address)
	}

	/// Returns `true` if `property` is settable
	/// - parameter property: The property to query
	/// - throws: An error if `self` does not have `property`
	public final func isPropertySettable(_ property: PropertyAddress) throws -> Bool {
		var address = property.rawValue
//		guard AudioObjectHasProperty(objectID, &address) else {
//			return false
//		}

		var settable: DarwinBoolean = false
		let result = AudioObjectIsPropertySettable(objectID, &address, &settable)
		guard result == kAudioHardwareNoError else {
			throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
		}

		return settable.boolValue
	}

	/// Registers `block` to be performed when `property` changes
	/// - parameter property: The property to observe
	/// - parameter block: A closure to invoke when `property` changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public final func whenPropertyChanges(_ property: PropertyAddress, perform block: (() -> Void)?) throws {
		var propertyAddress = property.rawValue

		// Remove the existing listener block, if any, for the property
		if let listenerBlock = listenerBlocks[property] {
			let result = AudioObjectRemovePropertyListenerBlock(objectID, &propertyAddress, DispatchQueue.global(qos: .background), listenerBlock)
			guard result == kAudioHardwareNoError else {
				os_log(.error, log: audioObjectLog, "AudioObjectRemovePropertyListenerBlock (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
				throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
			}
		}

		if let block = block {
			let listenerBlock: AudioObjectPropertyListenerBlock = { inNumberAddresses, inAddresses in
//				let buf = UnsafeBufferPointer(start: inAddresses, count: Int(inNumberAddresses))
				block()
			}

			listenerBlocks[property] = listenerBlock;

			let result = AudioObjectAddPropertyListenerBlock(objectID, &propertyAddress, DispatchQueue.global(qos: .background), listenerBlock)
			guard result == kAudioHardwareNoError else {
				os_log(.error, log: audioObjectLog, "AudioObjectAddPropertyListenerBlock (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
				throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
			}
		}
	}
}

// MARK: - Scalar Property Retrieval

extension AudioObject {
	/// Returns the value of `property`
	/// - parameter property: The address of the desired property
	/// - parameter qualifier: An optional property qualifier
	/// - parameter initialValue: An optional initial value for `outData` when calling `AudioObjectGetPropertyData`
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty<T: Numeric>(_ property: PropertyAddress, qualifier: PropertyQualifier? = nil, initialValue: T = 0) throws -> T {
		var value = initialValue
		try readAudioObjectProperty(property, from: objectID, into: &value, qualifier: qualifier)
		return value
	}

	/// Returns the value of `property`
	/// - parameter property: The address of the desired property
	/// - parameter qualifier: An optional property qualifier
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty(_ property: PropertyAddress, qualifier: PropertyQualifier? = nil) throws -> String {
		var value: CFTypeRef = unsafeBitCast(0, to: CFTypeRef.self)
		try readAudioObjectProperty(property, from: objectID, into: &value, qualifier: qualifier)
		return value as! String
	}

	/// Returns the value of `property`
	/// - parameter property: The address of the desired property
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty(_ property: PropertyAddress, qualifier: PropertyQualifier? = nil) throws -> [AnyHashable: Any] {
		var value: CFTypeRef = unsafeBitCast(0, to: CFTypeRef.self)
		try readAudioObjectProperty(property, from: objectID, into: &value, qualifier: qualifier)
		return value as! [AnyHashable: Any]
	}

	/// Returns the value of `property`
	/// - parameter property: The address of the desired property
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty(_ property: PropertyAddress, qualifier: PropertyQualifier? = nil) throws -> URL {
		var value: CFTypeRef = unsafeBitCast(0, to: CFTypeRef.self)
		try readAudioObjectProperty(property, from: objectID, into: &value, qualifier: qualifier)
		return value as! URL
	}

	/// Returns the value of `property`
	/// - parameter property: The address of the desired property
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty(_ property: PropertyAddress) throws -> AudioValueRange {
		var value = AudioValueRange()
		try readAudioObjectProperty(property, from: objectID, into: &value)
		return value
	}

	/// Returns the value of `property`
	/// - parameter property: The address of the desired property
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty(_ property: PropertyAddress) throws -> AudioStreamBasicDescription {
		var value = AudioStreamBasicDescription()
		try readAudioObjectProperty(property, from: objectID, into: &value)
		return value
	}
}

// MARK: - Scalar Property Setting

extension AudioObject {
	/// Sets the value of `property` to `value`
	/// - parameter property: The address of the desired property
	/// - parameter value: The desired value
	/// - throws: An error if `self` does not have `property`, `property` is not settable, or the property value could not be set
	public func setProperty<T>(_ property: PropertyAddress, to value: T) throws {
		var data = value
		try writeAudioObjectProperty(property, on: objectID, from: &data)
	}
}

// MARK: - Array Properties

extension AudioObject {
	/// Returns the value of `property`
	/// - parameter property: The address of the desired property
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty<T>(_ property: PropertyAddress) throws -> [T] {
		let dataSize = try audioObjectPropertySize(property, from: objectID)
		let count = dataSize / MemoryLayout<T>.stride
		let array = try [T](unsafeUninitializedCapacity: count) { (buffer, initializedCount) in
			try readAudioObjectProperty(property, from: objectID, into: buffer.baseAddress!, size: dataSize)
			initializedCount = count
		}
		return array
	}

	/// Sets the value of `property` to `value`
	/// - parameter property: The address of the desired property
	/// - parameter value: The desired value
	/// - throws: An error if `self` does not have `property`, `property` is not settable, or the property value could not be set
	public func setProperty<T>(_ property: PropertyAddress, to value: [T]) throws {
		var data = value
		let dataSize = MemoryLayout<T>.stride * value.count
		try writeAudioObjectProperty(property, on: objectID, from: &data, size: dataSize)
	}
}

// MARK: - Base Audio Object Properties

extension AudioObject {
	/// Returns the base class of the underlying HAL audio object
	/// - remark: This corresponds to the property `kAudioObjectPropertyBaseClass`
	public func baseClass() throws -> AudioClassID {
		return try getProperty(PropertyAddress(kAudioObjectPropertyBaseClass))
	}

	/// Returns the class of the underlying HAL audio object
	/// - remark: This corresponds to the property `kAudioObjectPropertyClass`
	public func `class`() throws -> AudioClassID {
		return try getProperty(PropertyAddress(kAudioObjectPropertyClass))
	}

	/// Returns the audio object's owning object
	/// - remark: This corresponds to the property `kAudioObjectPropertyOwner`
	/// - note: The system audio object
	/// - remark: This corresponds to the property `kAudioObjectSystemObject` does not have an owner
	public func owner() throws -> AudioObject {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioObjectPropertyOwner)))
	}

	/// Returns the audio object's name
	/// - remark: This corresponds to the property `kAudioObjectPropertyName`
	public func name() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyName))
	}

	/// Returns the audio object's model name
	/// - remark: This corresponds to the property `kAudioObjectPropertyModelName`
	public func modelName() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyModelName))
	}

	/// Returns the audio object's manufacturer
	/// - remark: This corresponds to the property `kAudioObjectPropertyManufacturer`
	public func manufacturer() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyManufacturer))
	}

	/// Returns the name of `element`
	/// - remark: This corresponds to the property `kAudioObjectPropertyElementName`
	/// - parameter element: The desired element
	/// - parameter scope: The desired scope
	public func nameOfElement(_ element: PropertyElement, in scope: PropertyScope = .global) throws -> String {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioObjectPropertyElementName), scope: scope, element: element))
	}

	/// Returns the category name of `element` in `scope`
	/// - remark: This corresponds to the property `kAudioObjectPropertyElementCategoryName`
	/// - parameter element: The desired element
	/// - parameter scope: The desired scope
	public func categoryNameOfElement(_ element: PropertyElement, in scope: PropertyScope = .global) throws -> String {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioObjectPropertyElementCategoryName), scope: scope, element: element))
	}

	/// Returns the number name of `element`
	/// - remark: This corresponds to the property `kAudioObjectPropertyElementNumberName`
	public func numberNameOfElement(_ element: PropertyElement, in scope: PropertyScope = .global) throws -> String {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioObjectPropertyElementNumberName), scope: scope, element: element))
	}

	/// Returns the audio objects owned by `self`
	/// - remark: This corresponds to the property `kAudioObjectPropertyOwnedObjects`
	public func ownedObjects() throws -> [AudioObject] {
		return try getProperty(PropertyAddress(kAudioObjectPropertyOwnedObjects)).map { AudioObject.make($0) }
	}

	/// Returns `true` if the audio object's hardware is drawing attention to itself
	/// - remark: This corresponds to the property `kAudioObjectPropertyIdentify`
	public func identify() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioObjectPropertyIdentify)) as UInt32 != 0
	}
	/// Sets whether the audio object's hardware should draw attention to itself
	/// - remark: This corresponds to the property `kAudioObjectPropertyIdentify`
	/// - parameter value: Whether the audio hardware should draw attention to itself
	public func setIdentify(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioObjectPropertyIdentify), to: UInt32(value ? 1 : 0))
	}

	/// Returns the audio object's serial number
	/// - remark: This corresponds to the property `kAudioObjectPropertySerialNumber`
	public func serialNumber() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertySerialNumber))
	}

	/// Returns the audio object's firmware version
	/// - remark: This corresponds to the property `kAudioObjectPropertyFirmwareVersion`
	public func firmwareVersion() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyFirmwareVersion))
	}
}

extension AudioObject: CustomDebugStringConvertible {
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)) \"\(name)\">"
		}
		else {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false))>"
		}
	}
}

// MARK: - Helpers

extension AudioObjectPropertyAddress: Hashable {
	public static func == (lhs: AudioObjectPropertyAddress, rhs: AudioObjectPropertyAddress) -> Bool {
		return lhs.mSelector == rhs.mSelector && lhs.mScope == rhs.mScope && lhs.mElement == rhs.mElement
		// Congruence?
//		return ((lhs.mSelector == rhs.mSelector) 	|| (lhs.mSelector == kAudioObjectPropertySelectorWildcard) 	|| (rhs.mSelector == kAudioObjectPropertySelectorWildcard))
//			&& ((lhs.mScope == rhs.mScope) 			|| (lhs.mScope == kAudioObjectPropertyScopeWildcard) 		|| (rhs.mScope == kAudioObjectPropertyScopeWildcard))
//			&& ((lhs.mElement == rhs.mElement) 		|| (lhs.mElement == kAudioObjectPropertyElementWildcard) 	|| (rhs.mElement == kAudioObjectPropertyElementWildcard))
	}

	public func hash(into hasher: inout Hasher) {
		hasher.combine(mSelector)
		hasher.combine(mScope)
		hasher.combine(mElement)
	}
}

/// Returns the value of `kAudioObjectPropertyClass` for `objectID` or `0` on error
func AudioObjectClass(_ objectID: AudioObjectID) -> AudioClassID {
	do {
		var value: AudioClassID = 0
		try readAudioObjectProperty(PropertyAddress(kAudioObjectPropertyClass), from: objectID, into: &value)
		return value
	}
	catch {
		return 0
	}
}

/// Returns the value of `kAudioObjectPropertyBaseClass` for `objectID` or `0` on error
func AudioObjectBaseClass(_ objectID: AudioObjectID) -> AudioClassID {
	do {
		var value: AudioClassID = 0
		try readAudioObjectProperty(PropertyAddress(kAudioObjectPropertyBaseClass), from: objectID, into: &value)
		return value
	}
	catch {
		return 0
	}
}

/// Returns `true` if an audio object's class is equal to `classID`
func AudioObjectIsClass(_ objectID: AudioObjectID, _ classID: AudioClassID) -> Bool
{
	return AudioObjectClass(objectID) == classID
}

/// Returns `true` if an audio object's class or base class is equal to `classID`
func AudioObjectIsClassOrSubclassOf(_ objectID: AudioObjectID, _ classID: AudioClassID) -> Bool
{
	return AudioObjectClass(objectID) == classID || AudioObjectBaseClass(objectID) == classID
}

/// The log for `AudioObject` and subclasses
let audioObjectLog = OSLog(subsystem: "org.sbooth.AudioEngine", category: "AudioObject")

// MARK: - AudioObject Creation

// Class clusters in the Objective-C sense can't be implemented in Swift
// since Swift initializers don't return a value.
//
// Ideally `AudioObject.init(_ objectID: AudioObjectID)` would initialize and return
// the appropriate subclass, but since that isn't possible,
// `AudioObject.init(_ objectID: AudioObjectID)` has internal access and
// the factory method `AudioObject.make(_ objectID: AudioObjectID)` is public.

extension AudioObject {
	/// Creates and returns an initialized `AudioObject`
	///
	/// Whenever possible this will return a specialized subclass exposing additional functionality
	/// - parameter objectID: The audio object ID
	public class func make(_ objectID: AudioObjectID) -> AudioObject {
		if objectID == kAudioObjectSystemObject {
			return AudioSystemObject.instance
		}

		let objectClass = AudioObjectClass(objectID)
		let objectBaseClass = AudioObjectBaseClass(objectID)

		switch objectBaseClass {
		case kAudioObjectClassID:
			switch objectClass {
			case kAudioBoxClassID: 			return AudioBox(objectID)
			case kAudioClockDeviceClassID: 	return AudioClockDevice(objectID)
			case kAudioControlClassID: 		return AudioControl(objectID)
			case kAudioDeviceClassID: 		return AudioDevice(objectID)
			case kAudioPlugInClassID: 		return AudioPlugIn(objectID)
			case kAudioStreamClassID: 		return AudioStream(objectID)
			default:
				os_log(.debug, log: audioObjectLog, "Unknown audio object class '%{public}@'", objectClass.fourCC)
				return AudioObject(objectID)
			}

		case kAudioControlClassID:
			switch objectClass {
			case kAudioBooleanControlClassID:		return BooleanControl(objectID)
			case kAudioLevelControlClassID:			return LevelControl(objectID)
			case kAudioSelectorControlClassID: 		return SelectorControl(objectID)
			case kAudioSliderControlClassID:		return SliderControl(objectID)
			case kAudioStereoPanControlClassID: 	return StereoPanControl(objectID)
			default:
				os_log(.debug, log: audioObjectLog, "Unknown audio control class '%{public}@'", objectClass.fourCC)
				return AudioControl(objectID)
			}

		case kAudioBooleanControlClassID:
			switch objectClass {
			case kAudioMuteControlClassID: 			return MuteControl(objectID)
			case kAudioSoloControlClassID:			return SoloControl(objectID)
			case kAudioJackControlClassID:			return JackControl(objectID)
			case kAudioLFEMuteControlClassID:		return LFEMuteControl(objectID)
			case kAudioPhantomPowerControlClassID:	return PhantomPowerControl(objectID)
			case kAudioPhaseInvertControlClassID:	return PhaseInvertControl(objectID)
			case kAudioClipLightControlClassID:		return ClipLightControl(objectID)
			case kAudioTalkbackControlClassID:		return TalkbackControl(objectID)
			case kAudioListenbackControlClassID: 	return ListenbackControl(objectID)
			default:
				os_log(.debug, log: audioObjectLog, "Unknown boolean control class '%{public}@'", objectClass.fourCC)
				return BooleanControl(objectID)
			}

		case kAudioLevelControlClassID:
			switch objectClass {
			case kAudioVolumeControlClassID: 		return VolumeControl(objectID)
			case kAudioLFEVolumeControlClassID: 	return LFEVolumeControl(objectID)
			default:
				os_log(.debug, log: audioObjectLog, "Unknown level control class '%{public}@'", objectClass.fourCC)
				return LevelControl(objectID)
			}

		case kAudioSelectorControlClassID:
			switch objectClass {
			case kAudioDataSourceControlClassID: 		return DataSourceControl(objectID)
			case kAudioDataDestinationControlClassID: 	return DataDestinationControl(objectID)
			case kAudioClockSourceControlClassID: 		return ClockSourceControl(objectID)
			case kAudioLineLevelControlClassID: 		return LineLevelControl(objectID)
			case kAudioHighPassFilterControlClassID: 	return HighPassFilterControl(objectID)
			default:
				os_log(.debug, log: audioObjectLog, "Unknown selector control class '%{public}@'", objectClass.fourCC)
				return SelectorControl(objectID)
			}

		case kAudioDeviceClassID:
			switch objectClass {
			case kAudioAggregateDeviceClassID: 	return AudioAggregateDevice(objectID)
			case kAudioEndPointDeviceClassID:	return AudioEndpointDevice(objectID)
			case kAudioEndPointClassID:			return AudioEndpoint(objectID)
			case kAudioSubDeviceClassID:		return AudioSubdevice(objectID)
			default:
				os_log(.debug, log: audioObjectLog, "Unknown audio device class '%{public}@'", objectClass.fourCC)
				return AudioDevice(objectID)
			}

		case kAudioPlugInClassID:
			switch objectClass {
			case kAudioTransportManagerClassID: 	return AudioTransportManager(objectID)
			default:
				os_log(.debug, log: audioObjectLog, "Unknown audio plug-in class '%{public}@'", objectClass.fourCC)
				return AudioPlugIn(objectID)
			}

		default:
			os_log(.debug, log: audioObjectLog, "Unknown audio object base class '%{public}@'", objectClass.fourCC)
			return AudioObject(objectID)
		}
	}
}
