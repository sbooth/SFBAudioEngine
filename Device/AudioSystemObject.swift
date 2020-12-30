//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// The HAL audio system object (`kAudioSystemObjectClassID`)
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
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
	/// Returns `true` if audio devices should mix stereo to mono (`kAudioHardwarePropertyMixStereoToMono`)
	public func mixStereoToMono() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyMixStereoToMono)) as UInt32 != 0
	}
	/// Sets whether audio devices should mix stereo to mono (`kAudioHardwarePropertyMixStereoToMono`)
	public func setMixStereoToMono(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyMixStereoToMono), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the current process contains the master HAL instance (`kAudioHardwarePropertyProcessIsMaster`)
	public func processIsMaster() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyProcessIsMaster)) as UInt32 != 0
	}

	/// Returns `true` if the HAL is initing or exiting the process (`kAudioHardwarePropertyIsInitingOrExiting`)
	public func isInitingOrExiting() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyIsInitingOrExiting)) as UInt32 != 0
	}

	/// Informs the HAL the effective user id of the process has changed (`kAudioHardwarePropertyUserIDChanged`)
	public func setUserIDChanged() throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyUserIDChanged), to: 1)
	}

	/// Returns `true` if the process will be heard (`kAudioHardwarePropertyProcessIsAudible`)
	public func processIsAudible() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyProcessIsAudible)) as UInt32 != 0
	}
	/// Sets whether the process is audible (`kAudioHardwarePropertyProcessIsAudible`)
	public func setProcessIsAudible(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyProcessIsAudible), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the process will allow the CPU to sleep while audio IO is in progress (`kAudioHardwarePropertySleepingIsAllowed`)
	public func sleepingIsAllowed() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertySleepingIsAllowed)) as UInt32 != 0
	}
	/// Sets whether the process will allow the CPU to sleep while audio IO is in progress (`kAudioHardwarePropertySleepingIsAllowed`)
	public func setSleepingIsAllowed(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertySleepingIsAllowed), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the process should be unloaded after a period of inactivity (`kAudioHardwarePropertyUnloadingIsAllowed`)
	public func unloadingIsAllowed() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyUnloadingIsAllowed)) as UInt32 != 0
	}
	/// Sets whether the process should be unloaded after a period of inactivity (`kAudioHardwarePropertyUnloadingIsAllowed`)
	public func setUnloadingIsAllowed(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyUnloadingIsAllowed), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the HAL should automatically take hog mode on behalf of the process (`kAudioHardwarePropertyHogModeIsAllowed`)
	public func hogModeIsAllowed() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyHogModeIsAllowed)) as UInt32 != 0
	}
	/// Sets whether the HAL should automatically take hog mode on behalf of the process (`kAudioHardwarePropertyHogModeIsAllowed`)
	public func setHogModeIsAllowed(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyHogModeIsAllowed), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the login session of the user is a console or headless session (`kAudioHardwarePropertyUserSessionIsActiveOrHeadless`)
	public func userSessionIsActiveOrHeadless() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyUserSessionIsActiveOrHeadless)) as UInt32 != 0
	}

	/// Returns the power hint (`kAudioHardwarePropertyPowerHint`)
	public func powerHint() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioHardwarePropertyPowerHint)) as UInt32 != 0
	}
	/// Sets the power hint (`kAudioHardwarePropertyPowerHint`)
	public func setPowerHint(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioHardwarePropertyPowerHint), to: UInt32(value ? 1 : 0))
	}
}
