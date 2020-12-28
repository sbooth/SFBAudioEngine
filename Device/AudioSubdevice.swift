//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

public class AudioSubdevice: AudioDevice {
	/// A thin wrapper around a HAL audio device transport type
	public struct DriftCompensationQuality: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
		/// Minimum quality
		public static let min 		= TransportType(rawValue: kAudioSubDeviceDriftCompensationMinQuality)
		/// Low quality
		public static let low 		= TransportType(rawValue: kAudioSubDeviceDriftCompensationLowQuality)
		/// Medium quality
		public static let medium 	= TransportType(rawValue: kAudioSubDeviceDriftCompensationMediumQuality)
		/// High quality
		public static let high 		= TransportType(rawValue: kAudioSubDeviceDriftCompensationHighQuality)
		/// Maximum quality
		public static let max 		= TransportType(rawValue: kAudioSubDeviceDriftCompensationMaxQuality)

		public let rawValue: UInt32

		public init(rawValue: UInt32) {
			self.rawValue = rawValue
		}

		public init(integerLiteral value: UInt32) {
			self.rawValue = value
		}

		public init(stringLiteral value: StringLiteralType) {
			self.rawValue = value.fourCC
		}
	}
}

extension AudioSubdevice {
	/// Returns the extra latency (`kAudioSubDevicePropertyExtraLatency`)
	public func extraLatency() throws -> Double {
		return try getProperty(AudioObjectProperty(kAudioSubDevicePropertyExtraLatency))
	}

	/// Returns the drift compensation (`kAudioSubDevicePropertyDriftCompensation`)
	public func driftCompensation() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioSubDevicePropertyDriftCompensation)) != 0
	}
	/// Sets the drift compensation (`kAudioSubDevicePropertyDriftCompensation`)
	public func setDriftCompensation(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioSubDevicePropertyDriftCompensation), to: value ? 1 : 0)
	}

	/// Returns the drift compensation quality (`kAudioSubDevicePropertyDriftCompensationQuality`)
	public func driftCompensationQuality() throws -> UInt32 {
		return try getProperty(AudioObjectProperty(kAudioSubDevicePropertyDriftCompensationQuality))
	}
	/// Sets the drift compensation quality (`kAudioSubDevicePropertyDriftCompensationQuality`)
	public func setDriftCompensationQuality(_ value: UInt32) throws {
		try setProperty(AudioObjectProperty(kAudioSubDevicePropertyDriftCompensationQuality), to: value)
	}
}

extension AudioSubdevice.DriftCompensationQuality: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self.rawValue {
		case kAudioSubDeviceDriftCompensationMinQuality:			return "min"
		case kAudioSubDeviceDriftCompensationLowQuality:			return "low"
		case kAudioSubDeviceDriftCompensationMediumQuality: 		return "medium"
		case kAudioSubDeviceDriftCompensationHighQuality:			return "high"
		case kAudioSubDeviceDriftCompensationMaxQuality:			return "max"
		default:													return "\(self.rawValue)"
		}
	}
}
