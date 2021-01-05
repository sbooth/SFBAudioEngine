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
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyBoxList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioBox }
	}

	/// Returns an initialized `AudioBox` with `uid` or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateUIDToBox` on `kAudioObjectSystemObject`
	/// - parameter uid: The desired box UID
	public class func makeBox(forUID uid: String) throws -> AudioBox? {
		var qualifier = uid as CFString
		let objectID = try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToBox), type: AudioObjectID.self, qualifier: PropertyQualifier(&qualifier))
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
		return try getProperty(PropertyAddress(kAudioBoxPropertyBoxUID), type: CFString.self) as String
	}

	/// Returns the transport type
	/// - remark: This corresponds to the property `kAudioBoxPropertyTransportType`
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(PropertyAddress(kAudioBoxPropertyTransportType), type: UInt32.self))
	}

	/// Returns `true` if the box has audio
	/// - remark: This corresponds to the property `kAudioBoxPropertyHasAudio`
	public func hasAudio() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasAudio), type: UInt32.self) != 0
	}

	/// Returns `true` if the box has video
	/// - remark: This corresponds to the property `kAudioBoxPropertyHasVideo`
	public func hasVideo() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasVideo), type: UInt32.self) != 0
	}

	/// Returns `true` if the box has MIDI
	/// - remark: This corresponds to the property `kAudioBoxPropertyHasMIDI`
	public func hasMIDI() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyHasMIDI), type: UInt32.self) != 0
	}

	/// Returns `true` if the box is acquired
	/// - remark: This corresponds to the property `kAudioBoxPropertyAcquired`
	public func acquired() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBoxPropertyAcquired), type: UInt32.self) != 0
	}

	/// Returns the audio devices provided by the box
	/// - remark: This corresponds to the property `kAudioBoxPropertyDeviceList`
	public func deviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioBoxPropertyDeviceList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the audio clock devices provided by the box
	/// - remark: This corresponds to the property `kAudioBoxPropertyClockDeviceList`
	public func clockDeviceList() throws -> [AudioClockDevice] {
		return try getProperty(PropertyAddress(kAudioBoxPropertyClockDeviceList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioClockDevice }
	}
}

extension AudioBox {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: AudioObjectSelector<AudioBox>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<AudioBox>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<AudioBox>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObjectSelector where T == AudioBox {
	/// The property selector `kAudioBoxPropertyBoxUID`
	public static let boxUID = AudioObjectSelector(kAudioBoxPropertyBoxUID)
	/// The property selector `kAudioBoxPropertyTransportType`
	public static let transportType = AudioObjectSelector(kAudioBoxPropertyTransportType)
	/// The property selector `kAudioBoxPropertyHasAudio`
	public static let hasAudio = AudioObjectSelector(kAudioBoxPropertyHasAudio)
	/// The property selector `kAudioBoxPropertyHasVideo`
	public static let hasVideo = AudioObjectSelector(kAudioBoxPropertyHasVideo)
	/// The property selector `kAudioBoxPropertyHasMIDI`
	public static let hasMIDI = AudioObjectSelector(kAudioBoxPropertyHasMIDI)
	/// The property selector `kAudioBoxPropertyAcquired`
	public static let acquired = AudioObjectSelector(kAudioBoxPropertyAcquired)
	/// The property selector `kAudioBoxPropertyDeviceList`
	public static let deviceList = AudioObjectSelector(kAudioBoxPropertyDeviceList)
	/// The property selector `kAudioBoxPropertyClockDeviceList`
	public static let clockDeviceList = AudioObjectSelector(kAudioBoxPropertyClockDeviceList)
}
