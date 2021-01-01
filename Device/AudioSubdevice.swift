//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio subdevice
/// - remark: This class correponds to objects with base class `kAudioSubDeviceClassID`
public class AudioSubdevice: AudioDevice {
}

extension AudioSubdevice {
	/// Returns the extra latency
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyExtraLatency`
	public func extraLatency() throws -> Double {
		return try getProperty(PropertyAddress(kAudioSubDevicePropertyExtraLatency))
	}

	/// Returns the drift compensation
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensation`
	public func driftCompensation() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensation)) as UInt32 != 0
	}
	/// Sets the drift compensation
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensation`
	public func setDriftCompensation(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensation), to: UInt32(value ? 1 : 0))
	}

	/// Returns the drift compensation quality
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensationQuality`
	public func driftCompensationQuality() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensationQuality))
	}
	/// Sets the drift compensation quality
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensationQuality`
	public func setDriftCompensationQuality(_ value: UInt32) throws {
		try setProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensationQuality), to: value)
	}
}

extension AudioSubdevice {
	/// A thin wrapper around a HAL audio device transport type
	public struct DriftCompensationQuality: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
		/// Minimum quality
		public static let min 		= DriftCompensationQuality(rawValue: kAudioSubDeviceDriftCompensationMinQuality)
		/// Low quality
		public static let low 		= DriftCompensationQuality(rawValue: kAudioSubDeviceDriftCompensationLowQuality)
		/// Medium quality
		public static let medium 	= DriftCompensationQuality(rawValue: kAudioSubDeviceDriftCompensationMediumQuality)
		/// High quality
		public static let high 		= DriftCompensationQuality(rawValue: kAudioSubDeviceDriftCompensationHighQuality)
		/// Maximum quality
		public static let max 		= DriftCompensationQuality(rawValue: kAudioSubDeviceDriftCompensationMaxQuality)

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

extension AudioSubdevice.DriftCompensationQuality: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self.rawValue {
		case kAudioSubDeviceDriftCompensationMinQuality:			return "Minimum"
		case kAudioSubDeviceDriftCompensationLowQuality:			return "Low"
		case kAudioSubDeviceDriftCompensationMediumQuality: 		return "Medium"
		case kAudioSubDeviceDriftCompensationHighQuality:			return "High"
		case kAudioSubDeviceDriftCompensationMaxQuality:			return "Maximum"
		default:													return "\(self.rawValue)"
		}
	}
}
