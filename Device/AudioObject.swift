//
// Copyright (c) 2020 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CoreAudio
import os.log

/// A HAL audio object
public class AudioObject: CustomDebugStringConvertible {
	/// The underlying audio object ID
	public let objectID: AudioObjectID

	/// Initializes an `AudioObject` with `objectID`
	/// - precondition: `objectID` != `kAudioObjectUnknown`
	/// - parameter objectID: The HAL audio object ID
	init(_ objectID: AudioObjectID) {
		precondition(objectID != kAudioObjectUnknown)
		self.objectID = objectID
	}

	/// Registered audio object property listeners
	private var listenerBlocks = [PropertyAddress: AudioObjectPropertyListenerBlock]()

	/// Removes all property listeners
	/// - note: Errors are logged but otherwise ignored
	func removeAllPropertyListeners() {
		for (property, listenerBlock) in listenerBlocks {
			var address = property.rawValue
			let result = AudioObjectRemovePropertyListenerBlock(objectID, &address, .global(qos: .background), listenerBlock)
			if result != kAudioHardwareNoError {
				os_log(.error, log: audioObjectLog, "AudioObjectRemovePropertyListenerBlock (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
			}
		}
	}

	deinit {
		removeAllPropertyListeners()
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

		var settable: DarwinBoolean = false
		let result = AudioObjectIsPropertySettable(objectID, &address, &settable)
		guard result == kAudioHardwareNoError else {
			os_log(.error, log: audioObjectLog, "AudioObjectIsPropertySettable (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
			let userInfo = [NSLocalizedDescriptionKey: NSLocalizedString("Mutability information for the property \(property.selector) in scope \(property.scope) on audio object 0x\(String(objectID, radix: 16, uppercase: false)) could not be retrieved.", comment: "")]
			throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: userInfo)
		}

		return settable.boolValue
	}

	/// A block called with one or more changed audio object properties
	/// - parameter changes: An array of changed property addresses
	public typealias PropertyChangeNotificationBlock = (_ changes: [PropertyAddress]) -> Void

	/// Registers `block` to be performed when `property` changes
	/// - parameter property: The property to observe
	/// - parameter block: A closure to invoke when `property` changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public final func whenPropertyChanges(_ property: PropertyAddress, perform block: PropertyChangeNotificationBlock?) throws {
		var address = property.rawValue

		// Remove the existing listener block, if any, for the property
		if let listenerBlock = listenerBlocks.removeValue(forKey: property) {
			let result = AudioObjectRemovePropertyListenerBlock(objectID, &address, DispatchQueue.global(qos: .background), listenerBlock)
			guard result == kAudioHardwareNoError else {
				os_log(.error, log: audioObjectLog, "AudioObjectRemovePropertyListenerBlock (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
				let userInfo = [NSLocalizedDescriptionKey: NSLocalizedString("The listener block for the property \(property.selector) on audio object 0x\(String(objectID, radix: 16, uppercase: false)) could not be removed.", comment: "")]
				throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: userInfo)
			}
		}

		if let block = block {
			let listenerBlock: AudioObjectPropertyListenerBlock = { inNumberAddresses, inAddresses in
				let count = Int(inNumberAddresses)
				let addresses = UnsafeBufferPointer(start: inAddresses, count: count)
				let array = [PropertyAddress](unsafeUninitializedCapacity: count) { (buffer, initializedCount) in
					for i in 0 ..< count {
						buffer[i] = PropertyAddress(addresses[i])
					}
					initializedCount = count
				}
				block(array)
			}

			let result = AudioObjectAddPropertyListenerBlock(objectID, &address, DispatchQueue.global(qos: .background), listenerBlock)
			guard result == kAudioHardwareNoError else {
				os_log(.error, log: audioObjectLog, "AudioObjectAddPropertyListenerBlock (0x%x, %{public}@) failed: '%{public}@'", objectID, property.description, UInt32(result).fourCC)
				let userInfo = [NSLocalizedDescriptionKey: NSLocalizedString("The listener block for the property \(property.selector) on audio object 0x\(String(objectID, radix: 16, uppercase: false)) could not be added.", comment: "")]
				throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: userInfo)
			}

			listenerBlocks[property] = listenerBlock;
		}
	}

	// A textual representation of this instance, suitable for debugging.
	public var debugDescription: String {
		return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false))>"
	}
}

extension AudioObject: Hashable {
	public static func == (lhs: AudioObject, rhs: AudioObject) -> Bool {
		return lhs.objectID == rhs.objectID
	}

	public func hash(into hasher: inout Hasher) {
		hasher.combine(objectID)
	}
}

// MARK: - Scalar Properties

extension AudioObject {
	/// Returns the numeric value of `property`
	/// - note: The underlying audio object property must be backed by an equivalent native C type of `T`
	/// - parameter property: The address of the desired property
	/// - parameter type: The underlying numeric type
	/// - parameter qualifier: An optional property qualifier
	/// - parameter initialValue: An optional initial value for `outData` when calling `AudioObjectGetPropertyData`
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty<T: Numeric>(_ property: PropertyAddress, type: T.Type, qualifier: PropertyQualifier? = nil, initialValue: T = 0) throws -> T {
		return try getAudioObjectProperty(property, from: objectID, type: type, qualifier: qualifier, initialValue: initialValue)
	}

	/// Returns the Core Foundation object value of `property`
	/// - note: The underlying audio object property must be backed by a Core Foundation object and return a `CFType` with a +1 retain count
	/// - parameter property: The address of the desired property
	/// - parameter type: The underlying `CFType`
	/// - parameter qualifier: An optional property qualifier
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty<T: CFTypeRef>(_ property: PropertyAddress, type: T.Type, qualifier: PropertyQualifier? = nil) throws -> T {
		return try getAudioObjectProperty(property, from: objectID, type: type, qualifier: qualifier)
	}

	/// Returns the `AudioValueRange` value of `property`
	/// - note: The underlying audio object property must be backed by `AudioValueRange`
	/// - parameter property: The address of the desired property
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty(_ property: PropertyAddress) throws -> AudioValueRange {
		var value = AudioValueRange()
		try readAudioObjectProperty(property, from: objectID, into: &value)
		return value
	}

	/// Returns the `AudioStreamBasicDescription` value of `property`
	/// - note: The underlying audio object property must be backed by `AudioStreamBasicDescription`
	/// - parameter property: The address of the desired property
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty(_ property: PropertyAddress) throws -> AudioStreamBasicDescription {
		var value = AudioStreamBasicDescription()
		try readAudioObjectProperty(property, from: objectID, into: &value)
		return value
	}

	/// Sets the value of `property` to `value`
	/// - note: The underlying audio object property must be backed by `T`
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
	/// Returns the array value of `property`
	/// - note: The underlying audio object property must be backed by a C array of `T`
	/// - parameter property: The address of the desired property
	/// - parameter type: The underlying array element type
	/// - parameter qualifier: An optional property qualifier
	/// - throws: An error if `self` does not have `property` or the property value could not be retrieved
	public func getProperty<T>(_ property: PropertyAddress, elementType type: T.Type, qualifier: PropertyQualifier? = nil) throws -> [T] {
		return try getAudioObjectProperty(property, from: objectID, elementType: type, qualifier: qualifier)
	}

	/// Sets the value of `property` to `value`
	/// - note: The underlying audio object property must be backed by a C array of `T`
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
	/// Returns the bundle ID of the plug-in that instantiated the object
	/// - remark: This corresponds to the property `kAudioObjectPropertyCreator`
	public func creator() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyCreator), type: CFString.self) as String
	}

	// kAudioObjectPropertyListenerAdded and kAudioObjectPropertyListenerRemoved omitted

	/// Returns the base class of the underlying HAL audio object
	/// - remark: This corresponds to the property `kAudioObjectPropertyBaseClass`
	public func baseClass() throws -> AudioClassID {
		return try getProperty(PropertyAddress(kAudioObjectPropertyBaseClass), type: AudioClassID.self)
	}

	/// Returns the class of the underlying HAL audio object
	/// - remark: This corresponds to the property `kAudioObjectPropertyClass`
	public func `class`() throws -> AudioClassID {
		return try getProperty(PropertyAddress(kAudioObjectPropertyClass), type: AudioClassID.self)
	}

	/// Returns the audio object's owning object
	/// - remark: This corresponds to the property `kAudioObjectPropertyOwner`
	/// - note: The system audio object does not have an owner
	public func owner() throws -> AudioObject {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioObjectPropertyOwner), type: AudioObjectID.self))
	}

	/// Returns the audio object's name
	/// - remark: This corresponds to the property `kAudioObjectPropertyName`
	public func name() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyName), type: CFString.self) as String
	}

	/// Returns the audio object's model name
	/// - remark: This corresponds to the property `kAudioObjectPropertyModelName`
	public func modelName() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyModelName), type: CFString.self) as String
	}

	/// Returns the audio object's manufacturer
	/// - remark: This corresponds to the property `kAudioObjectPropertyManufacturer`
	public func manufacturer() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyManufacturer), type: CFString.self) as String
	}

	/// Returns the name of `element`
	/// - remark: This corresponds to the property `kAudioObjectPropertyElementName`
	/// - parameter element: The desired element
	/// - parameter scope: The desired scope
	public func nameOfElement(_ element: PropertyElement, inScope scope: PropertyScope = .global) throws -> String {
		return try getProperty(PropertyAddress(PropertySelector(kAudioObjectPropertyElementName), scope: scope, element: element), type: CFString.self) as String
	}

	/// Returns the category name of `element` in `scope`
	/// - remark: This corresponds to the property `kAudioObjectPropertyElementCategoryName`
	/// - parameter element: The desired element
	/// - parameter scope: The desired scope
	public func categoryNameOfElement(_ element: PropertyElement, inScope scope: PropertyScope = .global) throws -> String {
		return try getProperty(PropertyAddress(PropertySelector(kAudioObjectPropertyElementCategoryName), scope: scope, element: element), type: CFString.self) as String
	}

	/// Returns the number name of `element`
	/// - remark: This corresponds to the property `kAudioObjectPropertyElementNumberName`
	public func numberNameOfElement(_ element: PropertyElement, inScope scope: PropertyScope = .global) throws -> String {
		return try getProperty(PropertyAddress(PropertySelector(kAudioObjectPropertyElementNumberName), scope: scope, element: element), type: CFString.self) as String
	}

	/// Returns the audio objects owned by `self`
	/// - remark: This corresponds to the property `kAudioObjectPropertyOwnedObjects`
	/// - parameter type: An optional array of `AudioClassID`s to which the returned objects will be restricted
	public func ownedObjects(ofType type: [AudioClassID]? = nil) throws -> [AudioObject] {
		if type != nil {
			var qualifierData = type!
			let qualifierDataSize = MemoryLayout<AudioClassID>.stride * type!.count
			let qualifier = PropertyQualifier(value: &qualifierData, size: UInt32(qualifierDataSize))
			return try getProperty(PropertyAddress(kAudioObjectPropertyOwnedObjects), elementType: AudioObjectID.self, qualifier: qualifier).map { AudioObject.make($0) }
		}
		return try getProperty(PropertyAddress(kAudioObjectPropertyOwnedObjects), elementType: AudioObjectID.self).map { AudioObject.make($0) }
	}

	/// Returns `true` if the audio object's hardware is drawing attention to itself
	/// - remark: This corresponds to the property `kAudioObjectPropertyIdentify`
	public func identify() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioObjectPropertyIdentify), type: UInt32.self) != 0
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
		return try getProperty(PropertyAddress(kAudioObjectPropertySerialNumber), type: CFString.self) as String
	}

	/// Returns the audio object's firmware version
	/// - remark: This corresponds to the property `kAudioObjectPropertyFirmwareVersion`
	public func firmwareVersion() throws -> String {
		return try getProperty(PropertyAddress(kAudioObjectPropertyFirmwareVersion), type: CFString.self) as String
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
	/// - precondition: `objectID` != `kAudioObjectUnknown`
	/// - parameter objectID: The audio object ID
	public class func make(_ objectID: AudioObjectID) -> AudioObject {
		precondition(objectID != kAudioObjectUnknown)

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
			case kAudioVolumeControlClassID: 			return VolumeControl(objectID)
			case kAudioLFEVolumeControlClassID: 		return LFEVolumeControl(objectID)
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
			case kAudioSubDeviceClassID:		return AudioSubDevice(objectID)
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

// MARK: -

/// A thin wrapper around a HAL audio object property selector for a specific `AudioObject` subclass
public struct AudioObjectSelector<T: AudioObject> {
	/// The underlying `AudioObjectPropertySelector` value
	let rawValue: AudioObjectPropertySelector

	/// Creates a new instance with the specified value
	/// - parameter value: The value to use for the new instance
	init(_ value: AudioObjectPropertySelector) {
		self.rawValue = value
	}
}

extension AudioObject {
	/// Returns `true` if `self` has `selector` in `scope` on `element`
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public func hasSelector(_ selector: AudioObjectSelector<AudioObject>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .main) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Returns `true` if `selector` in `scope` on `element` is settable
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<AudioObject>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .main) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Registers `block` to be performed when `selector` in `scope` on `element` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<AudioObject>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .main, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element), perform: block)
	}
}

extension AudioObjectSelector where T == AudioObject {
	/// The wildcard property selector `kAudioObjectPropertySelectorWildcard`
	public static let wildcard = AudioObjectSelector(kAudioObjectPropertySelectorWildcard)

	/// The property selector `kAudioObjectPropertyCreator`
	public static let creator = AudioObjectSelector(kAudioObjectPropertyCreator)
	// kAudioObjectPropertyListenerAdded and kAudioObjectPropertyListenerRemoved omitted

	/// The property selector `kAudioObjectPropertyBaseClass`
	public static let baseClass = AudioObjectSelector(kAudioObjectPropertyBaseClass)
	/// The property selector `kAudioObjectPropertyClass`
	public static let `class` = AudioObjectSelector(kAudioObjectPropertyClass)
	/// The property selector `kAudioObjectPropertyOwner`
	public static let owner = AudioObjectSelector(kAudioObjectPropertyOwner)
	/// The property selector `kAudioObjectPropertyName`
	public static let name = AudioObjectSelector(kAudioObjectPropertyName)
	/// The property selector `kAudioObjectPropertyModelName`
	public static let modelName = AudioObjectSelector(kAudioObjectPropertyModelName)
	/// The property selector `kAudioObjectPropertyManufacturer`
	public static let manufacturer = AudioObjectSelector(kAudioObjectPropertyManufacturer)
	/// The property selector `kAudioObjectPropertyElementName`
	public static let elementName = AudioObjectSelector(kAudioObjectPropertyElementName)
	/// The property selector `kAudioObjectPropertyElementCategoryName`
	public static let elementCategoryName = AudioObjectSelector(kAudioObjectPropertyElementCategoryName)
	/// The property selector `kAudioObjectPropertyElementNumberName`
	public static let elementNumberName = AudioObjectSelector(kAudioObjectPropertyElementNumberName)
	/// The property selector `kAudioObjectPropertyOwnedObjects`
	public static let ownedObjects = AudioObjectSelector(kAudioObjectPropertyOwnedObjects)
	/// The property selector `kAudioObjectPropertyIdentify`
	public static let identify = AudioObjectSelector(kAudioObjectPropertyIdentify)
	/// The property selector `kAudioObjectPropertySerialNumber`
	public static let serialNumber = AudioObjectSelector(kAudioObjectPropertySerialNumber)
	/// The property selector `kAudioObjectPropertyFirmwareVersion`
	public static let firmwareVersion = AudioObjectSelector(kAudioObjectPropertyFirmwareVersion)
}

extension AudioObjectSelector: CustomStringConvertible {
	public var description: String {
		return "\(type(of: T.self)): '\(rawValue.fourCC)'"
	}
}
