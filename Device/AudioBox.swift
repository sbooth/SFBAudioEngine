//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio box object
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
/// - remark: This class correponds to objects with base class `kAudioBoxClassID`
public class AudioBox: AudioObject {
	/// Returns the available audio boxes
	/// - remark: This corresponds to the property`kAudioHardwarePropertyBoxList` on `kAudioObjectSystemObject`
	public class func boxes() throws -> [AudioBox] {
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyBoxList)).map { AudioObject.make($0) as! AudioBox }
	}

	/// Returns an initialized `AudioBox` with `uid` or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateUIDToBox` on `kAudioObjectSystemObject`
	/// - parameter uid: The desired box UID
	public class func makeBox(forUID uid: String) throws -> AudioBox? {
		var qualifier = uid as CFString
		let objectID: AudioObjectID = try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToBox), qualifier: PropertyQualifier(&qualifier))
		guard objectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(objectID) as! AudioBox)
	}

	public override var debugDescription: String {
		do {
			var media = [String]()
			if try hasAudio() { media.append("audio") }
			if try hasVideo() { media.append("video") }
			if try hasMIDI() { media.append("MIDI") }
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), \(media.joined(separator: ", ")), [\(try deviceList().map({ $0.debugDescription }).joined(separator: ", "))]>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension AudioBox {
	/// Returns the box UID
	/// - remark: This corresponds to the property `kAudioBoxPropertyBoxUID`
	public func boxUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioBoxPropertyBoxUID))
	}

	/// Returns the transport type
	/// - remark: This corresponds to the property `kAudioBoxPropertyTransportType`
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(PropertyAddress(kAudioBoxPropertyTransportType)))
	}

	/// Returns `true` if the box has audio
	/// - remark: This corresponds to the property `kAudioBoxPropertyHasAudio`
	public func hasAudio() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasAudio)) as UInt32 != 0
	}

	/// Returns `true` if the box has video
	/// - remark: This corresponds to the property `kAudioBoxPropertyHasVideo`
	public func hasVideo() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasVideo)) as UInt32 != 0
	}

	/// Returns `true` if the box has MIDI
	/// - remark: This corresponds to the property `kAudioBoxPropertyHasMIDI`
	public func hasMIDI() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasMIDI)) as UInt32 != 0
	}

	/// Returns `true` if the box is acquired
	/// - remark: This corresponds to the property `kAudioBoxPropertyAcquired`
	public func acquired() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyAcquired)) as UInt32 != 0
	}

	/// Returns the audio devices provided by the box
	/// - remark: This corresponds to the property `kAudioBoxPropertyDeviceList`
	public func deviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioBoxPropertyDeviceList)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the audio clock devices provided by the box
	/// - remark: This corresponds to the property `kAudioBoxPropertyClockDeviceList`
	public func clockDeviceList() throws -> [AudioClockDevice] {
		return try getProperty(PropertyAddress(kAudioBoxPropertyClockDeviceList)).map { AudioObject.make($0) as! AudioClockDevice }
	}
}
