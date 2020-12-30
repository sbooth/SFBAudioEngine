//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio clock device object
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
/// - remark: This class correponds to objects with base class `kAudioClockDeviceClassID`
public class AudioClockDevice: AudioObject {
	/// Returns the available audio clock devices
	/// - remark: This corresponds to the property`kAudioHardwarePropertyClockDeviceList` on `kAudioObjectSystemObject`
	public class func clockDevices() throws -> [AudioClockDevice] {
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyClockDeviceList)).map { AudioObject.make($0) as! AudioClockDevice }
	}

	/// Returns an initialized `AudioClockDevice` with `uid` or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateUIDToClockDevice` on `kAudioObjectSystemObject`
	/// - parameter uid: The desired clock device UID
	public class func makeClockDevice(_ uid: String) throws -> AudioClockDevice? {
		var qualifier = uid as CFString
		let objectID: AudioObjectID = try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToClockDevice), qualifier: PropertyQualifier(&qualifier))
		guard objectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(objectID) as! AudioClockDevice)
	}

	/// Initializes an `AudioClockDevice` with `uid`
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateUIDToClockDevice` on `kAudioObjectSystemObject`
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
	/// Returns the clock device UID
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyDeviceUID`
	public func clockDeviceUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyDeviceUID))
	}

	/// Returns the transport type
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyTransportType`
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(PropertyAddress(kAudioClockDevicePropertyTransportType)))
	}

	/// Returns the domain
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyClockDomain`
	public func domain() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyClockDomain))
	}

	/// Returns `true` if the clock device is alive
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyDeviceIsAlive`
	public func isAlive() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyDeviceIsAlive)) as UInt32 != 0
	}

	/// Returns `true` if the clock device is running
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyClockDomain`
	public func isRunning() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyClockDomain)) as UInt32 != 0
	}

	/// Returns the latency
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyDeviceIsRunning`
	public func latency() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyDeviceIsRunning))
	}

	/// Returns the audio controls owned by `self`
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyControlList`
	public func controlList() throws -> [AudioControl] {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyControlList)).map { AudioObject.make($0) as! AudioControl }
	}

	/// Returns the sample rate
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyNominalSampleRate`
	public func sampleRate() throws -> Double {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyNominalSampleRate))
	}

	/// Returns the available sample rates
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyAvailableNominalSampleRates`
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyAvailableNominalSampleRates))
	}
}
