//
// Copyright (c) 2020 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CoreAudio
import os.log

/// A HAL audio aggregate device object
/// - remark: This class correponds to objects with the base class `kAudioAggregateDeviceClassID`
public class AudioAggregateDevice: AudioDevice {
	/// Creates and returns a new `AudioAggregateDevice` using the provided description
	/// - parameter description: A dictionary specifying how to build the `AudioAggregateDevice`
	/// - returns: A newly-created `AudioAggregateDevice`
	/// - throws: An error if the `AudioAggregateDevice` could not be created
	public static func create(description: [AnyHashable: Any]) throws -> AudioAggregateDevice {
		var objectID: AudioObjectID = kAudioObjectUnknown
		let result = AudioHardwareCreateAggregateDevice(description as CFDictionary, &objectID)
		guard result == kAudioHardwareNoError else {
			os_log(.error, log: audioObjectLog, "AudioHardwareCreateAggregateDevice (%{public}@) failed: '%{public}@'", description, UInt32(result).fourCC)
			throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
		}
		return AudioAggregateDevice(objectID)
	}

	public func destroy() throws {
		removeAllPropertyListeners()
	}

	/// Destroys `device`
	/// - note: Futher use of `device` following this function is undefined
	/// - parameter device: The `AudioAggregateDevice` to destroy
	/// - throws: An error if the `AudioAggregateDevice` could not be destroyed
	public static func destroy(_ device: AudioAggregateDevice) throws {
		let result = AudioHardwareDestroyAggregateDevice(device.objectID)
		guard result == kAudioHardwareNoError else {
			os_log(.error, log: audioObjectLog, "AudioHardwareDestroyAggregateDevice (0x%x) failed: '%{public}@'", device.objectID, UInt32(result).fourCC)
			throw NSError(domain: NSOSStatusErrorDomain, code: Int(result), userInfo: nil)
		}
		device.removeAllPropertyListeners()
	}
}

extension AudioAggregateDevice {
	/// Returns the UIDs of all subdevices in the aggregate device, active or inactive
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyFullSubDeviceList`
	public func fullSubdeviceList() throws -> [String] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyFullSubDeviceList), type: CFArray.self) as! [String]
	}

	/// Returns the active subdevices in the aggregate device
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyActiveSubDeviceList`
	public func activeSubdeviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyActiveSubDeviceList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the composition
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyComposition`
	public func composition() throws -> [AnyHashable: Any] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyComposition), type: CFDictionary.self) as! [AnyHashable: Any]
	}

	/// Returns the master subdevice
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyMasterSubDevice`
	public func masterSubdevice() throws -> AudioDevice {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioAggregateDevicePropertyMasterSubDevice), type: AudioObjectID.self)) as! AudioDevice
	}

	/// Returns the clock device
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyClockDevice`
	public func clockDevice() throws -> AudioClockDevice {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioAggregateDevicePropertyClockDevice), type: AudioObjectID.self)) as! AudioClockDevice
	}
	/// Sets the clock device
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyClockDevice`
	public func setClockDevice(_ value: AudioClockDevice) throws {
		try setProperty(PropertyAddress(kAudioAggregateDevicePropertyClockDevice), to: value.objectID)
	}
}

extension AudioAggregateDevice {
	/// Returns `true` if the aggregate device is private
	/// - remark: This corresponds to the value of `kAudioAggregateDeviceIsPrivateKey` in `composition()`
	public func isPrivate() throws -> Bool {
		let isPrivate = try composition()[kAudioAggregateDeviceIsPrivateKey] as? NSNumber
		return isPrivate?.boolValue ?? false
	}

	/// Returns `true` if the aggregate device is stacked
	/// - remark: This corresponds to the value of `kAudioAggregateDeviceIsStackedKey` in `composition()`
	public func isStacked() throws -> Bool {
		let isPrivate = try composition()[kAudioAggregateDeviceIsStackedKey] as? NSNumber
		return isPrivate?.boolValue ?? false
	}
}

extension AudioAggregateDevice {
	/// Returns `true` if `self` has `selector` in `scope` on `element`
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public func hasSelector(_ selector: AudioObjectSelector<AudioAggregateDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .main) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Returns `true` if `selector` in `scope` on `element` is settable
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<AudioAggregateDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .main) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Registers `block` to be performed when `selector` in `scope` on `element` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<AudioAggregateDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .main, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element), perform: block)
	}
}

extension AudioObjectSelector where T == AudioAggregateDevice {
	/// The property selector `kAudioAggregateDevicePropertyFullSubDeviceList`
	public static let fullSubDeviceList = AudioObjectSelector(kAudioAggregateDevicePropertyFullSubDeviceList)
	/// The property selector `kAudioAggregateDevicePropertyActiveSubDeviceList`
	public static let activeSubDeviceList = AudioObjectSelector(kAudioAggregateDevicePropertyActiveSubDeviceList)
	/// The property selector `kAudioAggregateDevicePropertyComposition`
	public static let composition = AudioObjectSelector(kAudioAggregateDevicePropertyComposition)
	/// The property selector `kAudioAggregateDevicePropertyMasterSubDevice`
	public static let masterSubDevice = AudioObjectSelector(kAudioAggregateDevicePropertyMasterSubDevice)
	/// The property selector `kAudioAggregateDevicePropertyClockDevice`
	public static let clockDevice = AudioObjectSelector(kAudioAggregateDevicePropertyClockDevice)
}
