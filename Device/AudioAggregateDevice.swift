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
		var value: CFTypeRef = unsafeBitCast(0, to: CFTypeRef.self)
		try readAudioObjectProperty(PropertyAddress(kAudioAggregateDevicePropertyFullSubDeviceList), from: objectID, into: &value)
		return value as! [String]
	}

	/// Returns the active subdevices in the aggregate device
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyActiveSubDeviceList`
	public func activeSubdeviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyActiveSubDeviceList)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the composition
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyComposition`
	public func composition() throws -> [AnyHashable: Any] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyComposition))
	}

	/// Returns the master subdevice
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyMasterSubDevice`
	public func masterSubdevice() throws -> AudioDevice {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioAggregateDevicePropertyMasterSubDevice))) as! AudioDevice
	}

	/// Returns the clock device
	/// - remark: This corresponds to the property `kAudioAggregateDevicePropertyClockDevice`
	public func clockDevice() throws -> AudioClockDevice {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioAggregateDevicePropertyClockDevice))) as! AudioClockDevice
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
