/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AggregateDevice {
	/// Returns the UIDs of all subdevices, active or inactive, in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyFullSubDeviceList`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: All subdevice UIDs
	/// - throws: An error if the property could not be retrieved
	public func allSubdevices(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [String] {
		return try getProperty(.aggregateDeviceFullSubDeviceList, scope: scope, element: element) as [Any] as! [String]
	}

	/// Returns the active subdevices in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyActiveSubDeviceList`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The active subdevices
	/// - throws: An error if the property could not be retrieved
	public func activeSubdevices(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioDevice] {
		return try getProperty(.aggregateDeviceActiveSubDeviceList, scope: scope, element: element) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the aggregate device's composition
	/// - note: This corresponds to `kAudioAggregateDevicePropertyComposition`
	/// @note The constants for the dictionary keys are located in \c AudioHardwareBase.h
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The aggregate device's composition
	/// - throws: An error if the property could not be retrieved
	public func composition(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AnyHashable: Any] {
		return try getProperty(.aggregateDeviceComposition, scope: scope, element: element)
	}

	/// Returns the active subdevices in the aggregate device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyMasterSubDevice`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The master subdevice
	/// - throws: An error if the property could not be retrieved
	public func masterSubdevice(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioDevice {
		return try getProperty(.aggregateDeviceMasterSubDevice, scope: scope, element: element) as AudioObject as! AudioDevice
	}

	/// Returns the aggregate device's clock device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyClockDevice`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The clock device
	/// - throws: An error if the property could not be retrieved
	public func clockDevice(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AudioDevice {
		return try getProperty(.aggregateDeviceClockDevice, scope: scope, element: element) as AudioObject as! AudioDevice
	}

	/// Sets the aggregate device's clock device
	/// - note: This corresponds to `kAudioAggregateDevicePropertyClockDevice`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be set
	public func setClockDevice(_ clockDevice: ClockDevice, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		return try setProperty(.aggregateDeviceClockDevice, clockDevice, scope: scope, element: element)
	}

	// MARK: - Convenience Accessors

	/// Returns `true` if the aggregate device is private
	/// - note: This returns the value of `kAudioAggregateDeviceIsPrivateKey` from `composition`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: `true` if the aggregate device is private
	/// - throws: An error if the property could not be retrieved
	public func isPrivate(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		guard let value: Bool = try composition(scope, element: element)[kAudioAggregateDeviceIsPrivateKey] as? Bool else {
			return false
		}
		return value
	}

	/// Returns `true` if the aggregate device is stacked
	/// - note: This returns the value of `kAudioAggregateDeviceIsStackedKey` from `composition`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: `true` if the aggregate device is stacked
	/// - throws: An error if the property could not be retrieved
	public func isStacked(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		guard let value: Bool = try composition(scope, element: element)[kAudioAggregateDeviceIsStackedKey] as? Bool else {
			return false
		}
		return value
	}
}
