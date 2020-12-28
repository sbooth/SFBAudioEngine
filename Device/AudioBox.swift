//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio box object (`kAudioBoxClassID`)
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
public class AudioBox: AudioObject {
	/// Returns the available audio boxes (`kAudioHardwarePropertyBoxList` from `kAudioObjectSystemObject`)
	public class func boxes() throws -> [AudioBox] {
		try getAudioObjectProperty(AudioObjectProperty(kAudioHardwarePropertyBoxList), from: AudioObjectID(kAudioObjectSystemObject)).map { AudioObject.make($0) as! AudioBox }
	}

	/// Initializes an `AudioBox` with `uid`
	/// - parameter uid: The desired box UID
	public convenience init?(_ uid: String) {
		var qualifier = uid as CFString
		guard let boxObjectID: AudioObjectID = try? getAudioObjectProperty(AudioObjectProperty(kAudioHardwarePropertyTranslateUIDToBox), from: AudioObjectID(kAudioObjectSystemObject), qualifier: PropertyQualifier(&qualifier)), boxObjectID != kAudioObjectUnknown else {
			return nil
		}
		self.init(boxObjectID)
	}
}

extension AudioBox {
	/// Returns the box UID (`kAudioBoxPropertyBoxUID`)
	public func boxUID() throws -> String {
		return try getProperty(AudioObjectProperty(kAudioBoxPropertyBoxUID))
	}

	/// Returns the transport type (`kAudioBoxPropertyTransportType`)
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(AudioObjectProperty(kAudioBoxPropertyTransportType)))
	}

	/// Returns `true` if the box has audio (`kAudioBoxPropertyHasAudio`)
	public func hasAudio() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioBoxPropertyHasAudio)) != 0
	}

	/// Returns `true` if the box has video (`kAudioBoxPropertyHasVideo`)
	public func hasVideo() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioBoxPropertyHasVideo)) != 0
	}

	/// Returns `true` if the box has MIDI (`kAudioBoxPropertyHasMIDI`)
	public func hasMIDI() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioBoxPropertyHasMIDI)) != 0
	}

	/// Returns `true` if the box is acquired (`kAudioBoxPropertyAcquired`)
	public func acquired() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioBoxPropertyAcquired)) != 0
	}

	/// Returns the audio devices provided by the box (`kAudioBoxPropertyDeviceList`)
	public func deviceList() throws -> [AudioDevice] {
		return try getProperty(AudioObjectProperty(kAudioBoxPropertyDeviceList)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the audio clock devices provided by the box (`kAudioBoxPropertyClockDeviceList`)
	public func clockDeviceList() throws -> [AudioClockDevice] {
		return try getProperty(AudioObjectProperty(kAudioBoxPropertyClockDeviceList)).map { AudioObject.make($0) as! AudioClockDevice }
	}
}
