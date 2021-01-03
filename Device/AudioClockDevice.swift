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
	public class func makeClockDevice(forUID uid: String) throws -> AudioClockDevice? {
		var qualifier = uid as CFString
		let objectID: AudioObjectID = try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToClockDevice), qualifier: PropertyQualifier(&qualifier))
		guard objectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(objectID) as! AudioClockDevice)
	}
}

extension AudioClockDevice {
	/// Returns the clock device UID
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyDeviceUID`
	public func deviceUID() throws -> String {
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
	public func availableSampleRates() throws -> [ClosedRange<Double>] {
		let value: [AudioValueRange] = try getProperty(PropertyAddress(kAudioClockDevicePropertyAvailableNominalSampleRates))
		return value.map { $0.mMinimum ... $0.mMaximum }
	}
}

extension AudioClockDevice {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: Selector<AudioClockDevice>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<AudioClockDevice>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<AudioClockDevice>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObject.Selector where T == AudioClockDevice {
	/// The property selector `kAudioClockDevicePropertyDeviceUID`
	public static let deviceUID = AudioObject.Selector(kAudioClockDevicePropertyDeviceUID)
	/// The property selector `kAudioClockDevicePropertyTransportType`
	public static let transportType = AudioObject.Selector(kAudioClockDevicePropertyTransportType)
	/// The property selector `kAudioClockDevicePropertyClockDomain`
	public static let clockDomain = AudioObject.Selector(kAudioClockDevicePropertyClockDomain)
	/// The property selector `kAudioClockDevicePropertyDeviceIsAlive`
	public static let deviceIsAlive = AudioObject.Selector(kAudioClockDevicePropertyDeviceIsAlive)
	/// The property selector `kAudioClockDevicePropertyDeviceIsRunning`
	public static let deviceIsRunning = AudioObject.Selector(kAudioClockDevicePropertyDeviceIsRunning)
	/// The property selector `kAudioClockDevicePropertyLatency`
	public static let latency = AudioObject.Selector(kAudioClockDevicePropertyLatency)
	/// The property selector `kAudioClockDevicePropertyControlList`
	public static let controlList = AudioObject.Selector(kAudioClockDevicePropertyControlList)
	/// The property selector `kAudioClockDevicePropertyNominalSampleRate`
	public static let nominalSampleRate = AudioObject.Selector(kAudioClockDevicePropertyNominalSampleRate)
	/// The property selector `kAudioClockDevicePropertyAvailableNominalSampleRates`
	public static let availableNominalSampleRates = AudioObject.Selector(kAudioClockDevicePropertyAvailableNominalSampleRates)
}
