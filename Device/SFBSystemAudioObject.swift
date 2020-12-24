/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SystemAudioObject {
	/// Returns `true` if audio devices should mix stereo to mono
	/// - note: This corresponds to `kAudioHardwarePropertyMixStereoToMono`
	/// - throws: An error if the property could not be retrieved
	public func mixStereoToMono() throws -> Bool {
		return try getProperty(.mixStereoToMono) != 0
	}

	/// Returns `true` if the current process contains the master HAL instance
	/// - note: This corresponds to `kAudioHardwarePropertyProcessIsMaster`
	/// - throws: An error if the property could not be retrieved
	public func processIsMaster() throws -> Bool {
		return try getProperty(.processIsMaster) != 0
	}

	/// Returns `true` if the HAL is initing or exiting the process
	/// - note: This corresponds to `kAudioHardwarePropertyIsInitingOrExiting`
	/// - throws: An error if the property could not be retrieved
	public func isInitingOrExiting() throws -> Bool {
		return try getProperty(.isInitingOrExiting) != 0
	}

	/// Returns `true` if the process will be heard
	/// - note: This corresponds to `kAudioHardwarePropertyProcessIsAudible`
	/// - throws: An error if the property could not be retrieved
	public func processIsAudible() throws -> Bool {
		return try getProperty(.processIsAudible) != 0
	}

	/// Returns `true` if the process will allow the CPU to sleep while audio IO is in progress
	/// - note: This corresponds to `kAudioHardwarePropertySleepingIsAllowed`
	/// - throws: An error if the property could not be retrieved
	public func sleepingIsAllowed() throws -> Bool {
		return try getProperty(.sleepingIsAllowed) != 0
	}

	/// Returns `true` if the process should be unloaded after a period of inactivity
	/// - note: This corresponds to `kAudioHardwarePropertyUnloadingIsAllowed`
	/// - throws: An error if the property could not be retrieved
	public func unloadingIsAllowed() throws -> Bool {
		return try getProperty(.unloadingIsAllowed) != 0
	}

	/// Returns `true` if the HAL should automatically take hog mode on behalf of the process
	/// - note: This corresponds to `kAudioHardwarePropertyHogModeIsAllowed`
	/// - throws: An error if the property could not be retrieved
	public func hogModeIsAllowed() throws -> Bool {
		return try getProperty(.hogModeIsAllowed) != 0
	}

	/// Returns `true` if the login session of the user is a console or headless session
	/// - note: This corresponds to `kAudioHardwarePropertyUserSessionIsActiveOrHeadless`
	/// - throws: An error if the property could not be retrieved
	public func userSessionIsActiveOrHeadless() throws -> Bool {
		return try getProperty(.userSessionIsActiveOrHeadless) != 0
	}

	/// Returns the power hint
	/// - note: This corresponds to `kAudioHardwarePropertyPowerHint`
	/// - throws: An error if the property could not be retrieved
	public func powerHint() throws -> AudioHardwarePowerHint {
		return AudioHardwarePowerHint(rawValue: try getProperty(.powerHint))!
	}
}
