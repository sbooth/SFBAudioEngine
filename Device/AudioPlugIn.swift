//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio plug-in object (`kAudioPlugInClassID`)
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
public class AudioPlugIn: AudioObject {
	/// Returns the available audio plug-ins (`kAudioHardwarePropertyPlugInList` from `kAudioObjectSystemObject`)
	public class func plugIns() throws -> [AudioPlugIn] {
		try getAudioObjectProperty(AudioObjectProperty(kAudioHardwarePropertyPlugInList), from: AudioObjectID(kAudioObjectSystemObject)).map { AudioObject.make($0) as! AudioPlugIn }
	}

	/// Initializes an `AudioPlugIn` with `bundleID`
	/// - parameter bundleID: The desired bundle ID
	public convenience init?(_ bundleID: String) {
		var qualifier = bundleID as CFString
		guard let plugInObjectID: AudioObjectID = try? getAudioObjectProperty(AudioObjectProperty(kAudioHardwarePropertyTranslateBundleIDToPlugIn), from: AudioObjectID(kAudioObjectSystemObject), qualifier: PropertyQualifier(&qualifier)), plugInObjectID != kAudioObjectUnknown else {
			return nil
		}
		self.init(plugInObjectID)
	}
}

extension AudioPlugIn {
	/// Creates and returns a new aggregate device (`kAudioPlugInCreateAggregateDevice`)
	/// - parameter composition: The composition of the new aggregate device
	/// - note: The constants for `composition` are defined in `AudioHardware.h`
	func createAggregateDevice(composition: [AnyHashable: Any]) throws -> AudioDevice {
		var qualifier = composition as CFDictionary
		return AudioObject.make(try getProperty(AudioObjectProperty(kAudioPlugInCreateAggregateDevice), qualifier: PropertyQualifier(&qualifier))) as! AudioDevice
	}

	/// Destroys an aggregate device (`kAudioPlugInDestroyAggregateDevice`)
	func destroyAggregateDevice(_ aggregateDevice: AudioDevice) throws {
		_ = try getProperty(AudioObjectProperty(kAudioPlugInDestroyAggregateDevice), initialValue: aggregateDevice.objectID)
	}

	/// Returns the plug-in's bundle ID (`kAudioPlugInPropertyBundleID`)
	public func bundleID() throws -> String {
		return try getProperty(AudioObjectProperty(kAudioPlugInPropertyBundleID))
	}

	/// Returns the audio devices provided by the plug-in (`kAudioPlugInPropertyDeviceList`)
	public func deviceList() throws -> [AudioDevice] {
		return try getProperty(AudioObjectProperty(kAudioPlugInPropertyDeviceList)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the audio device provided by the plug-in with the specified UID (`kAudioPlugInPropertyTranslateUIDToDevice`) or `nil` if unknown
	/// - parameter uid: The desired device UID
	public func device(_ uid: String) throws -> AudioDevice? {
		var qualifierData = uid as CFString
		guard let deviceObjectID: AudioObjectID = try? getProperty(AudioObjectProperty(kAudioPlugInPropertyTranslateUIDToDevice), qualifier: PropertyQualifier(&qualifierData)), deviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(deviceObjectID) as! AudioDevice)
	}

	/// Returns the audio boxes provided by the plug-in (`kAudioPlugInPropertyBoxList`)
	public func boxList() throws -> [AudioBox] {
		return try getProperty(AudioObjectProperty(kAudioPlugInPropertyBoxList)).map { AudioObject.make($0) as! AudioBox }
	}

	/// Returns the audio box provided by the plug-in with the specified UID (`kAudioPlugInPropertyTranslateUIDToBox`) or `nil` if unknown
	/// - parameter uid: The desired box UID
	public func box(_ uid: String) throws -> AudioBox? {
		var qualifierData = uid as CFString
		guard let boxObjectID: AudioObjectID = try? getProperty(AudioObjectProperty(kAudioPlugInPropertyTranslateUIDToBox), qualifier: PropertyQualifier(&qualifierData)), boxObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(boxObjectID) as! AudioBox)
	}

	/// Returns the clock devices provided by the plug-in (`kAudioPlugInPropertyClockDeviceList`)
	public func clockDeviceList() throws -> [AudioClockDevice] {
		return try getProperty(AudioObjectProperty(kAudioPlugInPropertyClockDeviceList)).map { AudioObject.make($0) as! AudioClockDevice }
	}

	/// Returns the audio clock device provided by the plug-in with the specified UID (`kAudioPlugInPropertyTranslateUIDToClockDevice`) or `nil` if unknown
	/// - parameter uid: The desired clock device UID
	public func clockDevice(_ uid: String) throws -> AudioClockDevice? {
		var qualifierData = uid as CFString
		guard let clockDeviceObjectID: AudioObjectID = try? getProperty(AudioObjectProperty(kAudioPlugInPropertyTranslateUIDToClockDevice), qualifier: PropertyQualifier(&qualifierData)), clockDeviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(clockDeviceObjectID) as! AudioClockDevice)
	}
}
