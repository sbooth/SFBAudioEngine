/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AggregateDevice {
	/// Returns the UIDs of all subdevices, active or inactive, in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyFullSubDeviceList`
	/// - throws: An error if the property could not be retrieved
	public func allSubdevices() throws -> [String] {
		return try getProperty(.aggregateDeviceFullSubDeviceList) as [Any] as! [String]
	}

	/// Returns the active subdevices in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyActiveSubDeviceList`
	/// - throws: An error if the property could not be retrieved
	public func activeSubdevices() throws -> [AudioDevice] {
		return try getProperty(.aggregateDeviceActiveSubDeviceList) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the aggregate device's composition
	/// - note: This corresponds to `kAudioAggregateDevicePropertyComposition`
	/// @note The constants for the dictionary keys are located in \c AudioHardwareBase.h
	/// - throws: An error if the property could not be retrieved
	public func composition() throws -> [AnyHashable: Any] {
		return try getProperty(.aggregateDeviceComposition)
	}

	/// Returns the active subdevices in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyMasterSubDevice`
	/// - throws: An error if the property could not be retrieved
	public func masterSubdevice() throws -> AudioDevice {
		return try getProperty(.aggregateDeviceMasterSubDevice) as AudioObject as! AudioDevice
	}

	/// Returns the aggregate device's clock device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyClockDevice`
	/// - throws: An error if the property could not be retrieved
	public func clockDevice() throws -> AudioDevice {
		return try getProperty(.aggregateDeviceClockDevice) as AudioObject as! AudioDevice
	}

	/// Sets the aggregate device's clock device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyClockDevice`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setClockDevice(_ clockDevice: ClockDevice) throws {
		return try setProperty(.aggregateDeviceClockDevice, clockDevice)
	}

	// MARK: - Convenience Accessors

	/// Returns `true` if the aggregate device is private
	/// - note: This returns the value of `kAudioAggregateDeviceIsPrivateKey` from `composition`
	/// - throws: An error if the property could not be retrieved
	public func isPrivate() throws -> Bool {
		guard let value: Bool = try composition()[kAudioAggregateDeviceIsPrivateKey] as? Bool else {
			return false
		}
		return value
	}

	/// Returns `true` if the aggregate device is stacked
	/// - note: This returns the value of `kAudioAggregateDeviceIsStackedKey` from `composition`
	/// - throws: An error if the property could not be retrieved
	public func isStacked() throws -> Bool {
		guard let value: Bool = try composition()[kAudioAggregateDeviceIsStackedKey] as? Bool else {
			return false
		}
		return value
	}
}
