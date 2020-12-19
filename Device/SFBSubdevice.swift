/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension Subdevice {
	/// Returns the extra latency
	/// - note: This corresponds to `kAudioSubDevicePropertyExtraLatency`
	public func extraLatency(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Double {
		return try getProperty(.subdeviceExtraLatency, scope: scope, element: element)
	}

	/// Returns the drift compensation
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftCompensation`
	public func driftCompensation(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.subdeviceDriftCompensation, scope: scope, element: element) != 0
	}

	/// Sets the drift compensation
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftCompensation`
	public func setDriftCompensation(_ value: Bool, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try setProperty(.subdeviceDriftCompensation, UInt(value ? 1 : 0), scope: scope, element: element)
	}

	/// Returns the drift compensation quality
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftQuality`
	public func driftCompensationQuality(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt {
		return try getProperty(.subdeviceDriftCompensationQuality, scope: scope, element: element)
	}

	/// Sets the drift compensation quality
	/// - note: This corresponds to `kAudioSubDevicePropertyDriftQuality`
	public func setDriftCompensationQuality(_ value: UInt, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try setProperty(.subdeviceDriftCompensationQuality, value, scope: scope, element: element)
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
