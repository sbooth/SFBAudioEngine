/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension Subdevice {
	/// Returns the extra latency
	/// - note: This corresponds to `kAudioSubDevicePropertyExtraLatency`
	/// - throws: An error if the property could not be retrieved
	public func extraLatency() throws -> Double {
		return try getProperty(.subdeviceExtraLatency)
	}

	/// Returns the drift compensation
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftCompensation`
	/// - throws: An error if the property could not be retrieved
	public func driftCompensation() throws -> Bool {
		return try getProperty(.subdeviceDriftCompensation) != 0
	}

	/// Sets the drift compensation
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftCompensation`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setDriftCompensation(_ value: Bool) throws {
		try setProperty(.subdeviceDriftCompensation, UInt(value ? 1 : 0))
	}

	/// Returns the drift compensation quality
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftQuality`
	/// - throws: An error if the property could not be retrieved
	public func driftCompensationQuality() throws -> UInt {
		return try getProperty(.subdeviceDriftCompensationQuality)
	}

	/// Sets the drift compensation quality
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftQuality`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setDriftCompensationQuality(_ value: UInt) throws {
		try setProperty(.subdeviceDriftCompensationQuality, value)
	}
}

extension Subdevice.DriftCompensationQuality: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .min:			return ".min"
		case .low:			return ".low"
		case .medium: 		return ".medium"
		case .high:			return ".high"
		case .max:			return ".max"
		@unknown default: 	return "UNKNOWN (\(self.rawValue))"
		}
	}
}
