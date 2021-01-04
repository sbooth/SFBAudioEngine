//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio aggregate device object
/// - remark: This class correponds to objects with the base class `kAudioAggregateDeviceClassID`
public class AudioAggregateDevice: AudioDevice {
}

extension AudioAggregateDevice {
	/// Returns the UIDs of all subdevices in the aggregate device, active or inactive
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyFullSubDeviceList`
	public func fullSubdeviceList() throws -> [String] {
		var value: CFTypeRef! = nil
		try readAudioObjectProperty(PropertyAddress(kAudioAggregateDevicePropertyFullSubDeviceList), from: objectID, into: &value)
		return value as! [String]
	}

	/// Returns the active subdevices in the aggregate device
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyActiveSubDeviceList`
	public func activeSubdeviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyActiveSubDeviceList), arrayType: AudioObjectID.self).map { AudioObject.make($0) as! AudioDevice }
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
	public func hasSelector(_ selector: Selector<AudioAggregateDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Returns `true` if `selector` in `scope` on `element` is settable
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<AudioAggregateDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Registers `block` to be performed when `selector` in `scope` on `element` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<AudioAggregateDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element), perform: block)
	}
}

extension Selector where T == AudioAggregateDevice {
	/// The property selector `kAudioAggregateDevicePropertyFullSubDeviceList`
	public static let fullSubDeviceList = Selector(kAudioAggregateDevicePropertyFullSubDeviceList)
	/// The property selector `kAudioAggregateDevicePropertyActiveSubDeviceList`
	public static let activeSubDeviceList = Selector(kAudioAggregateDevicePropertyActiveSubDeviceList)
	/// The property selector `kAudioAggregateDevicePropertyComposition`
	public static let composition = Selector(kAudioAggregateDevicePropertyComposition)
	/// The property selector `kAudioAggregateDevicePropertyMasterSubDevice`
	public static let masterSubDevice = Selector(kAudioAggregateDevicePropertyMasterSubDevice)
	/// The property selector `kAudioAggregateDevicePropertyClockDevice`
	public static let clockDevice = Selector(kAudioAggregateDevicePropertyClockDevice)
}
