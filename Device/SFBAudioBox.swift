/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioBox {
	/// Returns the box UID
	/// - note: This corresponds to `kAudioBoxPropertyBoxUID`
	func boxUID() throws -> String {
		return try stringForProperty(.boxUID)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioBoxPropertyTransportType`
	func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: UInt32(try uintForProperty(.boxTransportType)))!
	}

	/// Returns `true` if the  box has audio
	/// - note: This corresponds to `kAudioBoxPropertyHasAudio`
	func hasAudio() throws -> Bool {
		return try uintForProperty(.boxHasAudio) != 0
	}

	/// Returns `true` if the  box has video
	/// - note: This corresponds to `kAudioBoxPropertyHasVideo`
	func hasVideo() throws -> Bool {
		return try uintForProperty(.boxHasVideo) != 0
	}

	/// Returns `true` if the  box has MIDI
	/// - note: This corresponds to `kAudioBoxPropertyHasMIDI`
	func hasMIDI() throws -> Bool {
		return try uintForProperty(.boxHasMIDI) != 0
	}

	/// Returns `true` if the  box is acquired
	/// - note: This corresponds to `kAudioBoxPropertyAcquired`
	func acquired() throws -> Bool {
		return try uintForProperty(.boxAcquired) != 0
	}

	/// Returns the audio devices provided by the box
	/// - note: This corresponds to `kAudioBoxPropertyDeviceList`
	func devices() throws -> [AudioDevice] {
		return try audioObjectsForProperty(.boxDeviceList) as! [AudioDevice]
	}

	/// Returns the audio clock devices provided by the box
	/// - note: This corresponds to `kAudioBoxPropertyClockDeviceList`
	func clockDevices() throws -> [ClockDevice] {
		return try audioObjectsForProperty(.boxClockDeviceList) as! [ClockDevice]
	}
}
