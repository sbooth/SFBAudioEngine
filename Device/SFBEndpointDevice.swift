/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension EndpointDevice {
	/// Returns the endoint device's composition
	/// - note: This corresponds to `kAudioEndPointDevicePropertyComposition`
	/// - note: The constants for the dictionary keys are located in `AudioHardwareBase.h`
	/// - throws: An error if the property could not be retrieved
	public func composition() throws -> [AnyHashable: Any] {
		return try getProperty(.endpointDeviceComposition)
	}

	/// Returns the available endpoints
	/// - note: This corresponds to `kAudioEndPointDevicePropertyEndPointList`
	/// - throws: An error if the property could not be retrieved
	public func endpoints() throws -> [AudioEndpoint] {
		return try getProperty(.endpointDeviceEndPointList) as [AudioObject] as! [AudioEndpoint]
	}

	/// Returns the owning `pid_t` (`0` for public devices)
	/// - note: This corresponds to `kAudioEndPointDevicePropertyIsPrivate`
	/// - throws: An error if the property could not be retrieved
	public func isPrivate() throws -> pid_t {
		return pid_t(bitPattern: try getProperty(.endpointDeviceIsPrivate))
	}
}
