//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio aggregate device object (`kAudioAggregateDeviceClassID`)
public class AudioAggregateDevice: AudioDevice {
}

extension AudioAggregateDevice {
	/// Returns the UIDs of all subdevices in the aggregate device, active or inactive (`kAudioAggregateDevicePropertyFullSubDeviceList`)
	public func fullSubdeviceList() throws -> [String] {
		var value: CFTypeRef = unsafeBitCast(0, to: CFTypeRef.self)
		try readAudioObjectProperty(PropertyAddress(kAudioAggregateDevicePropertyFullSubDeviceList), from: objectID, into: &value)
		return value as! [String]
	}

	/// Returns the active subdevices in the aggregate device (`kAudioAggregateDevicePropertyActiveSubDeviceList`)
	public func activeSubdeviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyActiveSubDeviceList)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the composition (`kAudioAggregateDevicePropertyComposition`)
	public func composition() throws -> [AnyHashable: Any] {
		return try getProperty(PropertyAddress(kAudioAggregateDevicePropertyComposition))
	}

	/// Returns the master subdevice (`kAudioAggregateDevicePropertyMasterSubDevice`)
	public func masterSubdevice() throws -> AudioDevice {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioAggregateDevicePropertyMasterSubDevice))) as! AudioDevice
	}

	/// Returns the clock device (`kAudioAggregateDevicePropertyClockDevice`)
	public func clockDevice() throws -> AudioClockDevice {
		return AudioObject.make(try getProperty(PropertyAddress(kAudioAggregateDevicePropertyClockDevice))) as! AudioClockDevice
	}
	/// Sets the clock device (`kAudioAggregateDevicePropertyClockDevice`)
	public func setClockDevice(_ value: AudioClockDevice) throws {
		try setProperty(PropertyAddress(kAudioAggregateDevicePropertyClockDevice), to: value.objectID)
	}
}

extension AudioAggregateDevice {
	/// Returns `true` if the aggregate device is private (`kAudioAggregateDeviceIsPrivateKey` in `self.composition()`)
	public func isPrivate() throws -> Bool {
		let isPrivate = try composition()[kAudioAggregateDeviceIsPrivateKey] as? NSNumber
		return isPrivate?.boolValue ?? false
	}

	/// Returns `true` if the aggregate device is stacked (`kAudioAggregateDeviceIsStackedKey` in `self.composition()`)
	public func isStacked() throws -> Bool {
		let isPrivate = try composition()[kAudioAggregateDeviceIsStackedKey] as? NSNumber
		return isPrivate?.boolValue ?? false
	}
}
