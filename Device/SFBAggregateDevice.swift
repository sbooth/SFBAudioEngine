/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AggregateDevice {
	/// Returns the UIDs of all subdevices, active or inactive, in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyFullSubDeviceList`
	public func allSubdevices(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [String] {
		return try getProperty(.aggregateDeviceFullSubDeviceList, scope: scope, element: element) as [Any] as! [String]
	}

	/// Returns the active subdevices in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyActiveSubDeviceList`
	public func activeSubdevices(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioDevice] {
		return try getProperty(.aggregateDeviceActiveSubDeviceList, scope: scope, element: element) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the aggregate device's composition
	/// - note: This corresponds to `kAudioAggregateDevicePropertyComposition`
	/// @note The constants for the dictionary keys are located in \c AudioHardwareBase.h
	public func composition(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AnyHashable: Any] {
		return try getProperty(.aggregateDeviceComposition, scope: scope, element: element)
	}

	/// Returns the active subdevices in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyMasterSubDevice`
	public func masterSubdevice(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioDevice {
		return try getProperty(.aggregateDeviceMasterSubDevice, scope: scope, element: element) as AudioObject as! AudioDevice
	}

	/// Returns the aggregate device's clock device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyClockDevice`
	public func clockDevice(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioDevice {
		return try getProperty(.aggregateDeviceClockDevice, scope: scope, element: element) as AudioObject as! AudioDevice
	}

	/// Sets the aggregate device's clock device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyClockDevice`
	public func setClockDevice(_ clockDevice: ClockDevice, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		return try setProperty(.aggregateDeviceClockDevice, clockDevice, scope: scope, element: element)
	}

	// MARK: - Convenience Accessors

	/// Returns `true` if the aggregate device is private
	/// - note: This returns the value of `kAudioAggregateDeviceIsPrivateKey` from `composition`
	public func isPrivate(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		guard let value: Bool = try composition(scope, element: element)[kAudioAggregateDeviceIsPrivateKey] as? Bool else {
			return false
		}
		return value
	}

	/// Returns `true` if the aggregate device is stacked
	/// - note: This returns the value of `kAudioAggregateDeviceIsStackedKey` from `composition`
	public func isStacked(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		guard let value: Bool = try composition(scope, element: element)[kAudioAggregateDeviceIsStackedKey] as? Bool else {
			return false
		}
		return value
	}
}
