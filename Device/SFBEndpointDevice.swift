/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension EndpointDevice {
	/// Returns the endoint device's composition
	/// - note: This corresponds to `kAudioEndPointDevicePropertyComposition`
	/// - note: The constants for the dictionary keys are located in `AudioHardwareBase.h`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The endpoint device's composition
	/// - throws: An error if the property could not be retrieved
	public func composition(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AnyHashable: Any] {
		return try getProperty(.endpointDeviceComposition, scope: scope, element: element)
	}

	/// Returns the available endpoints
	/// - note: This corresponds to `kAudioEndPointDevicePropertyEndPointList`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The available endpoints
	/// - throws: An error if the property could not be retrieved
	public func endpoints(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioDevice] {
		return try getProperty(.endpointDeviceEndPointList, scope: scope, element: element) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the owning `pid_t` (`0` for public devices)
	/// - note: This corresponds to `kAudioEndPointDevicePropertyIsPrivate`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - returns: The owning `pid_t`
	/// - throws: An error if the property could not be retrieved
	public func isPrivate(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> pid_t {
		return pid_t(try getProperty(.endpointDeviceIsPrivate, scope: scope, element: element) as UInt)
	}
}
