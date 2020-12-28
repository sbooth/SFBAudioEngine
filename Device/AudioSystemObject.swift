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
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyMixStereoToMono)) != 0
	}
	/// Sets whether audio devices should mix stereo to mono (`kAudioHardwarePropertyMixStereoToMono`)
	public func setMixStereoToMono(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioHardwarePropertyMixStereoToMono), to: value ? 1 : 0)
	}

	/// Returns `true` if the current process contains the master HAL instance (`kAudioHardwarePropertyProcessIsMaster`)
	public func processIsMaster() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyProcessIsMaster)) != 0
	}

	/// Returns `true` if the HAL is initing or exiting the process (`kAudioHardwarePropertyIsInitingOrExiting`)
	public func isInitingOrExiting() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyIsInitingOrExiting)) != 0
	}

	/// Informs the HAL the effective user id of the process has changed (`kAudioHardwarePropertyUserIDChanged`)
	public func setUserIDChanged() throws {
		try setProperty(AudioObjectProperty(kAudioHardwarePropertyUserIDChanged), to: 1)
	}

	/// Returns `true` if the process will be heard (`kAudioHardwarePropertyProcessIsAudible`)
	public func processIsAudible() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyProcessIsAudible)) != 0
	}
	/// Sets whether the process is audible (`kAudioHardwarePropertyProcessIsAudible`)
	public func setProcessIsAudible(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioHardwarePropertyProcessIsAudible), to: value ? 1 : 0)
	}

	/// Returns `true` if the process will allow the CPU to sleep while audio IO is in progress (`kAudioHardwarePropertySleepingIsAllowed`)
	public func sleepingIsAllowed() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertySleepingIsAllowed)) != 0
	}
	/// Sets whether the process will allow the CPU to sleep while audio IO is in progress (`kAudioHardwarePropertySleepingIsAllowed`)
	public func setSleepingIsAllowed(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioHardwarePropertySleepingIsAllowed), to: value ? 1 : 0)
	}

	/// Returns `true` if the process should be unloaded after a period of inactivity (`kAudioHardwarePropertyUnloadingIsAllowed`)
	public func unloadingIsAllowed() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyUnloadingIsAllowed)) != 0
	}
	/// Sets whether the process should be unloaded after a period of inactivity (`kAudioHardwarePropertyUnloadingIsAllowed`)
	public func setUnloadingIsAllowed(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioHardwarePropertyUnloadingIsAllowed), to: value ? 1 : 0)
	}

	/// Returns `true` if the HAL should automatically take hog mode on behalf of the process (`kAudioHardwarePropertyHogModeIsAllowed`)
	public func hogModeIsAllowed() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyHogModeIsAllowed)) != 0
	}
	/// Sets whether the HAL should automatically take hog mode on behalf of the process (`kAudioHardwarePropertyHogModeIsAllowed`)
	public func setHogModeIsAllowed(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioHardwarePropertyHogModeIsAllowed), to: value ? 1 : 0)
	}

	/// Returns `true` if the login session of the user is a console or headless session (`kAudioHardwarePropertyUserSessionIsActiveOrHeadless`)
	public func userSessionIsActiveOrHeadless() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyUserSessionIsActiveOrHeadless)) != 0
	}

	/// Returns the power hint (`kAudioHardwarePropertyPowerHint`)
	public func powerHint() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioHardwarePropertyPowerHint)) != 0
	}
	/// Sets the power hint (`kAudioHardwarePropertyPowerHint`)
	public func setPowerHint(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioHardwarePropertyPowerHint), to: value ? 1 : 0)
	}
}
