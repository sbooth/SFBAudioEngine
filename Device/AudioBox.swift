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
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyBoxList)).map { AudioObject.make($0) as! AudioBox }
	}

	/// Initializes an `AudioBox` with `uid`
	/// - parameter uid: The desired box UID
	public convenience init?(_ uid: String) {
		var qualifier = uid as CFString
		guard let boxObjectID: AudioObjectID = try? AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToBox), qualifier: PropertyQualifier(&qualifier)), boxObjectID != kAudioObjectUnknown else {
			return nil
		}
		self.init(boxObjectID)
	}
}

extension AudioBox {
	/// Returns the box UID (`kAudioBoxPropertyBoxUID`)
	public func boxUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioBoxPropertyBoxUID))
	}

	/// Returns the transport type (`kAudioBoxPropertyTransportType`)
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(PropertyAddress(kAudioBoxPropertyTransportType)))
	}

	/// Returns `true` if the box has audio (`kAudioBoxPropertyHasAudio`)
	public func hasAudio() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasAudio)) != 0
	}

	/// Returns `true` if the box has video (`kAudioBoxPropertyHasVideo`)
	public func hasVideo() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasVideo)) != 0
	}

	/// Returns `true` if the box has MIDI (`kAudioBoxPropertyHasMIDI`)
	public func hasMIDI() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasMIDI)) != 0
	}

	/// Returns `true` if the box is acquired (`kAudioBoxPropertyAcquired`)
	public func acquired() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyAcquired)) != 0
	}

	/// Returns the audio devices provided by the box (`kAudioBoxPropertyDeviceList`)
	public func deviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioBoxPropertyDeviceList)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the audio clock devices provided by the box (`kAudioBoxPropertyClockDeviceList`)
	public func clockDeviceList() throws -> [AudioClockDevice] {
		return try getProperty(PropertyAddress(kAudioBoxPropertyClockDeviceList)).map { AudioObject.make($0) as! AudioClockDevice }
	}
}
