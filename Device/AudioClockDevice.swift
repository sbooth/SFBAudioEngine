//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio clock device object (`kAudioClockDeviceClassID`)
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
public class AudioClockDevice: AudioObject {
	/// Returns the available audio clock devices (`kAudioHardwarePropertyClockDeviceList` from `kAudioObjectSystemObject`)
	public class func boxes() throws -> [AudioClockDevice] {
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyClockDeviceList)).map { AudioObject.make($0) as! AudioClockDevice }
	}

	/// Initializes an `AudioClockDevice` with `uid`
	/// - parameter uid: The desired clock device UID
	public convenience init?(_ uid: String) {
		var qualifier = uid as CFString
		guard let clockDeviceObjectID: AudioObjectID = try? AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToClockDevice), qualifier: PropertyQualifier(&qualifier)), clockDeviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		self.init(clockDeviceObjectID)
	}
}

extension AudioClockDevice {
	/// Returns the clock device UID (`kAudioClockDevicePropertyDeviceUID`)
	public func clockDeviceUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyDeviceUID))
	}

	/// Returns the transport type (`kAudioClockDevicePropertyTransportType`)
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(PropertyAddress(kAudioClockDevicePropertyTransportType)))
	}

	/// Returns the domain (`kAudioClockDevicePropertyClockDomain`)
	public func domain() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyClockDomain))
	}

	/// Returns `true` if the clock device is alive (`kAudioClockDevicePropertyDeviceIsAlive`)
	public func isAlive() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyDeviceIsAlive)) != 0
	}

	/// Returns `true` if the clock device is running (`kAudioClockDevicePropertyClockDomain`)
	public func isRunning() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyClockDomain)) != 0
	}

	/// Returns the latency (`kAudioClockDevicePropertyDeviceIsRunning`)
	public func latency() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyDeviceIsRunning))
	}

	/// Returns the audio controls owned by `self` (`kAudioClockDevicePropertyControlList`)
	public func controlList() throws -> [AudioControl] {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyControlList)).map { AudioObject.make($0) as! AudioControl }
	}

	/// Returns the sample rate (`kAudioClockDevicePropertyNominalSampleRate`)
	public func sampleRate() throws -> Double {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyNominalSampleRate))
	}

	/// Returns the available sample rates (`kAudioClockDevicePropertyAvailableNominalSampleRates`)
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyAvailableNominalSampleRates))
	}
}
