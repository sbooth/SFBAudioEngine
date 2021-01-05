//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// The HAL audio system object
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
/// - remark: This class correponds to the object with id `kAudioObjectSystemObject` and class `kAudioSystemObjectClassID`
public class AudioSystemObject: AudioObject {
	/// The singleton audio system object
	public static var instance = AudioSystemObject()

	@available(*, unavailable, message: "Use instance instead")
	private override init(_ objectID: AudioObjectID) {
		fatalError()
	}

	/// Initializes an `AudioSystemObject` with the`kAudioObjectSystemObject` object ID
	private init() {
		super.init(AudioObjectID(kAudioObjectSystemObject))
	}
}

extension AudioSystemObject {
	/// Returns `true` if audio devices should mix stereo to mono
	/// - remark: This corresponds to the property `kAudioHardwarePropertyMixStereoToMono`
	public func mixStereoToMono() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyMixStereoToMono), type: UInt32.self) != 0
	}
	/// Sets whether audio devices should mix stereo to mono
	/// - remark: This corresponds to the property `kAudioHardwarePropertyMixStereoToMono`
	public func setMixStereoToMono(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyMixStereoToMono), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the current process contains the master HAL instance
	/// - remark: This corresponds to the property `kAudioHardwarePropertyProcessIsMaster`
	public func processIsMaster() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyProcessIsMaster), type: UInt32.self) != 0
	}

	/// Returns `true` if the HAL is initing or exiting the process
	/// - remark: This corresponds to the property `kAudioHardwarePropertyIsInitingOrExiting`
	public func isInitingOrExiting() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyIsInitingOrExiting), type: UInt32.self) != 0
	}

	/// Informs the HAL the effective user id of the process has changed
	/// - remark: This corresponds to the property `kAudioHardwarePropertyUserIDChanged`
	public func setUserIDChanged() throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyUserIDChanged), to: UInt32(1))
	}

	/// Returns `true` if the process will be heard
	/// - remark: This corresponds to the property `kAudioHardwarePropertyProcessIsAudible`
	public func processIsAudible() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyProcessIsAudible), type: UInt32.self) != 0
	}
	/// Sets whether the process is audible
	/// - remark: This corresponds to the property `kAudioHardwarePropertyProcessIsAudible`
	public func setProcessIsAudible(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyProcessIsAudible), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the process will allow the CPU to sleep while audio IO is in progress
	/// - remark: This corresponds to the property `kAudioHardwarePropertySleepingIsAllowed`
	public func sleepingIsAllowed() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertySleepingIsAllowed), type: UInt32.self) != 0
	}
	/// Sets whether the process will allow the CPU to sleep while audio IO is in progress
	/// - remark: This corresponds to the property `kAudioHardwarePropertySleepingIsAllowed`
	public func setSleepingIsAllowed(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertySleepingIsAllowed), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the process should be unloaded after a period of inactivity
	/// - remark: This corresponds to the property `kAudioHardwarePropertyUnloadingIsAllowed`
	public func unloadingIsAllowed() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyUnloadingIsAllowed), type: UInt32.self) != 0
	}
	/// Sets whether the process should be unloaded after a period of inactivity
	/// - remark: This corresponds to the property `kAudioHardwarePropertyUnloadingIsAllowed`
	public func setUnloadingIsAllowed(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyUnloadingIsAllowed), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the HAL should automatically take hog mode on behalf of the process
	/// - remark: This corresponds to the property `kAudioHardwarePropertyHogModeIsAllowed`
	public func hogModeIsAllowed() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyHogModeIsAllowed), type: UInt32.self) != 0
	}
	/// Sets whether the HAL should automatically take hog mode on behalf of the process
	/// - remark: This corresponds to the property `kAudioHardwarePropertyHogModeIsAllowed`
	public func setHogModeIsAllowed(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyHogModeIsAllowed), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the login session of the user is a console or headless session
	/// - remark: This corresponds to the property `kAudioHardwarePropertyUserSessionIsActiveOrHeadless`
	public func userSessionIsActiveOrHeadless() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyUserSessionIsActiveOrHeadless), type: UInt32.self) != 0
	}

	/// Returns the power hint
	/// - remark: This corresponds to the property `kAudioHardwarePropertyPowerHint`
	public func powerHint() throws -> AudioHardwarePowerHint {
		return AudioHardwarePowerHint(rawValue: try getProperty(PropertyAddress(kAudioHardwarePropertyPowerHint), type: UInt32.self))!
	}
	/// Sets the power hint
	/// - remark: This corresponds to the property `kAudioHardwarePropertyPowerHint`
	public func setPowerHint(_ value: AudioHardwarePowerHint) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyPowerHint), to: value.rawValue)
	}
}

extension AudioSystemObject {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: Selector<AudioSystemObject>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<AudioSystemObject>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<AudioSystemObject>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension Selector where T == AudioSystemObject {
	/// The property selector `kAudioHardwarePropertyDevices`
	public static let devices = Selector(kAudioHardwarePropertyDevices)
	/// The property selector `kAudioHardwarePropertyDefaultInputDevice`
	public static let defaultInputDevice = Selector(kAudioHardwarePropertyDefaultInputDevice)
	/// The property selector `kAudioHardwarePropertyDefaultOutputDevice`
	public static let defaultOutputDevice = Selector(kAudioHardwarePropertyDefaultOutputDevice)
	/// The property selector `kAudioHardwarePropertyDefaultSystemOutputDevice`
	public static let defaultSystemOutputDevice = Selector(kAudioHardwarePropertyDefaultSystemOutputDevice)
	/// The property selector `kAudioHardwarePropertyTranslateUIDToDevice`
	public static let translateUIDToDevice = Selector(kAudioHardwarePropertyTranslateUIDToDevice)
	/// The property selector `kAudioHardwarePropertyMixStereoToMono`
	public static let mixStereoToMono = Selector(kAudioHardwarePropertyMixStereoToMono)
	/// The property selector `kAudioHardwarePropertyPlugInList`
	public static let plugInList = Selector(kAudioHardwarePropertyPlugInList)
	/// The property selector `kAudioHardwarePropertyTranslateBundleIDToPlugIn`
	public static let translateBundleIDToPlugIn = Selector(kAudioHardwarePropertyTranslateBundleIDToPlugIn)
	/// The property selector `kAudioHardwarePropertyTransportManagerList`
	public static let transportManagerList = Selector(kAudioHardwarePropertyTransportManagerList)
	/// The property selector `kAudioHardwarePropertyTranslateBundleIDToTransportManager`
	public static let translateBundleIDToTransportManager = Selector(kAudioHardwarePropertyTranslateBundleIDToTransportManager)
	/// The property selector `kAudioHardwarePropertyBoxList`
	public static let boxList = Selector(kAudioHardwarePropertyBoxList)
	/// The property selector `kAudioHardwarePropertyTranslateUIDToBox`
	public static let translateUIDToBox = Selector(kAudioHardwarePropertyTranslateUIDToBox)
	/// The property selector `kAudioHardwarePropertyClockDeviceList`
	public static let clockDeviceList = Selector(kAudioHardwarePropertyClockDeviceList)
	/// The property selector `kAudioHardwarePropertyTranslateUIDToClockDevice`
	public static let translateUIDToClockDevice = Selector(kAudioHardwarePropertyTranslateUIDToClockDevice)
	/// The property selector `kAudioHardwarePropertyProcessIsMaster`
	public static let processIsMaster = Selector(kAudioHardwarePropertyProcessIsMaster)
	/// The property selector `kAudioHardwarePropertyIsInitingOrExiting`
	public static let isInitingOrExiting = Selector(kAudioHardwarePropertyIsInitingOrExiting)
	/// The property selector `kAudioHardwarePropertyUserIDChanged`
	public static let userIDChanged = Selector(kAudioHardwarePropertyUserIDChanged)
	/// The property selector `kAudioHardwarePropertyProcessIsAudible`
	public static let processIsAudible = Selector(kAudioHardwarePropertyProcessIsAudible)
	/// The property selector `kAudioHardwarePropertySleepingIsAllowed`
	public static let sleepingIsAllowed = Selector(kAudioHardwarePropertySleepingIsAllowed)
	/// The property selector `kAudioHardwarePropertyUnloadingIsAllowed`
	public static let unloadingIsAllowed = Selector(kAudioHardwarePropertyUnloadingIsAllowed)
	/// The property selector `kAudioHardwarePropertyHogModeIsAllowed`
	public static let hogModeIsAllowed = Selector(kAudioHardwarePropertyHogModeIsAllowed)
	/// The property selector `kAudioHardwarePropertyUserSessionIsActiveOrHeadless`
	public static let userSessionIsActiveOrHeadless = Selector(kAudioHardwarePropertyUserSessionIsActiveOrHeadless)
	/// The property selector `kAudioHardwarePropertyServiceRestarted`
	public static let serviceRestarted = Selector(kAudioHardwarePropertyServiceRestarted)
	/// The property selector `kAudioHardwarePropertyPowerHint`
	public static let powerHint = Selector(kAudioHardwarePropertyPowerHint)
}
