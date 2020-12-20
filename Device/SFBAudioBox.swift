/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioBox {
	/// Returns the box UID
	/// - note: This corresponds to `kAudioBoxPropertyBoxUID`
	/// - returns: The box UID
	/// - throws: An error if the property could not be retrieved
	public func boxUID() throws -> String {
		return try getProperty(.boxUID)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioBoxPropertyTransportType`
	/// - returns: The transport type
	/// - throws: An error if the property could not be retrieved
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: UInt32(try getProperty(.boxTransportType) as UInt))!
	}

	/// Returns `true` if the  box has audio
	/// - note: This corresponds to `kAudioBoxPropertyHasAudio`
	/// - returns: `true` if the box has audio
	/// - throws: An error if the property could not be retrieved
	public func hasAudio() throws -> Bool {
		return try getProperty(.boxHasAudio) != 0
	}

	/// Returns `true` if the  box has video
	/// - note: This corresponds to `kAudioBoxPropertyHasVideo`
	/// - returns: `true` if the box has video
	/// - throws: An error if the property could not be retrieved
	public func hasVideo() throws -> Bool {
		return try getProperty(.boxHasVideo) != 0
	}

	/// Returns `true` if the  box has MIDI
	/// - note: This corresponds to `kAudioBoxPropertyHasMIDI`
	/// - returns: `true` if the box has MIDI
	/// - throws: An error if the property could not be retrieved
	public func hasMIDI() throws -> Bool {
		return try getProperty(.boxHasMIDI) != 0
	}

	/// Returns `true` if the  box is acquired
	/// - note: This corresponds to `kAudioBoxPropertyAcquired`
	/// - returns: `true` if the box is acquired
	/// - throws: An error if the property could not be retrieved
	public func acquired() throws -> Bool {
		return try getProperty(.boxAcquired) != 0
	}

	/// Returns the audio devices provided by the box
	/// - note: This corresponds to `kAudioBoxPropertyDeviceList`
	/// - returns: The audio devices provided by the box
	/// - throws: An error if the property could not be retrieved
	public func devices() throws -> [AudioDevice] {
		return try getProperty(.boxDeviceList) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the audio clock devices provided by the box
	/// - note: This corresponds to `kAudioBoxPropertyClockDeviceList`
	/// - returns: The audio clock devices provided by the box
	/// - throws: An error if the property could not be retrieved
	public func clockDevices() throws -> [ClockDevice] {
		return try getProperty(.boxClockDeviceList) as [AudioObject] as! [ClockDevice]
	}
}
