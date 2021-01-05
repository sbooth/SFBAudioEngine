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
		return try getProperty(PropertyAddress(kAudioSubDevicePropertyExtraLatency), type: Double.self)
	}

	/// Returns the drift compensation
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensation`
	public func driftCompensation() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensation), type: UInt32.self) != 0
	}
	/// Sets the drift compensation
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensation`
	public func setDriftCompensation(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensation), to: UInt32(value ? 1 : 0))
	}

	/// Returns the drift compensation quality
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensationQuality`
	public func driftCompensationQuality() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensationQuality), type: UInt32.self)
	}
	/// Sets the drift compensation quality
	/// - remark: This corresponds to the property `kAudioSubDevicePropertyDriftCompensationQuality`
	public func setDriftCompensationQuality(_ value: UInt32) throws {
		try setProperty(PropertyAddress(kAudioSubDevicePropertyDriftCompensationQuality), to: value)
	}
}

extension AudioSubdevice {
	/// A thin wrapper around a HAL audio subdevice drift compensation quality setting
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

extension AudioSubdevice {
	/// Returns `true` if `self` has `selector` in `scope` on `element`
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public func hasSelector(_ selector: AudioObjectSelector<AudioSubdevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Returns `true` if `selector` in `scope` on `element` is settable
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<AudioSubdevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Registers `block` to be performed when `selector` in `scope` on `element` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<AudioSubdevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element), perform: block)
	}
}

extension AudioObjectSelector where T == AudioSubdevice {
	/// The property selector `kAudioSubDevicePropertyExtraLatency`
	public static let extraLatency = AudioObjectSelector(kAudioSubDevicePropertyExtraLatency)
	/// The property selector `kAudioSubDevicePropertyDriftCompensation`
	public static let driftCompensation = AudioObjectSelector(kAudioSubDevicePropertyDriftCompensation)
	/// The property selector `kAudioSubDevicePropertyDriftCompensationQuality`
	public static let driftCompensationQuality = AudioObjectSelector(kAudioSubDevicePropertyDriftCompensationQuality)
}
