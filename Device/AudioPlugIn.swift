//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio plug-in object
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
/// - remark: This class correponds to objects of type `kAudioPlugInClassID`
public class AudioPlugIn: AudioObject {
	/// Returns the available audio plug-ins
	/// - remark: This corresponds to the property`kAudioHardwarePropertyPlugInList` on `kAudioObjectSystemObject`
	public class func plugIns() throws -> [AudioPlugIn] {
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyPlugInList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioPlugIn }
	}

	/// Returns an initialized `AudioPlugIn` with `bundleID` or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateBundleIDToPlugIn` on `kAudioObjectSystemObject`
	/// - parameter bundleID: The desired bundle ID
	public class func makePlugIn(forBundleID bundleID: String) throws -> AudioPlugIn? {
		guard let objectID = try AudioSystemObject.instance.plugInID(forBundleID: bundleID) else {
			return nil
		}
		return (AudioObject.make(objectID) as! AudioPlugIn)
	}

	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), [\(try deviceList().map({ $0.debugDescription }).joined(separator: ", "))]>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension AudioPlugIn {
	/// Creates and returns a new aggregate device
	/// - remark: This corresponds to the property `kAudioPlugInCreateAggregateDevice`
	/// - parameter composition: The composition of the new aggregate device
	/// - note: The constants for `composition` are defined in `AudioHardware.h`
	func createAggregateDevice(composition: [AnyHashable: Any]) throws -> AudioAggregateDevice {
		var qualifier = composition as CFDictionary
		return AudioObject.make(try getProperty(PropertyAddress(kAudioPlugInCreateAggregateDevice), type: AudioObjectID.self, qualifier: PropertyQualifier(&qualifier))) as! AudioAggregateDevice
	}

	/// Destroys an aggregate device
	/// - remark: This corresponds to the property `kAudioPlugInDestroyAggregateDevice`
	func destroyAggregateDevice(_ aggregateDevice: AudioAggregateDevice) throws {
		_ = try getProperty(PropertyAddress(kAudioPlugInDestroyAggregateDevice), type: UInt32.self, initialValue: aggregateDevice.objectID)
	}

	/// Returns the plug-in's bundle ID
	/// - remark: This corresponds to the property `kAudioPlugInPropertyBundleID`
	public func bundleID() throws -> String {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyBundleID), type: CFString.self) as String
	}

	/// Returns the audio devices provided by the plug-in
	/// - remark: This corresponds to the property `kAudioPlugInPropertyDeviceList`
	public func deviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyDeviceList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the audio device provided by the plug-in with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioPlugInPropertyTranslateUIDToDevice`
	/// - parameter uid: The desired device UID
	public func device(forUID uid: String) throws -> AudioDevice? {
		var qualifierData = uid as CFString
		let deviceObjectID = try getProperty(PropertyAddress(kAudioPlugInPropertyTranslateUIDToDevice), type: AudioObjectID.self, qualifier: PropertyQualifier(&qualifierData))
		guard deviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(deviceObjectID) as! AudioDevice)
	}

	/// Returns the audio boxes provided by the plug-in
	/// - remark: This corresponds to the property `kAudioPlugInPropertyBoxList`
	public func boxList() throws -> [AudioBox] {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyBoxList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioBox }
	}

	/// Returns the audio box provided by the plug-in with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioPlugInPropertyTranslateUIDToBox`
	/// - parameter uid: The desired box UID
	public func box(forUID uid: String) throws -> AudioBox? {
		var qualifierData = uid as CFString
		let boxObjectID = try getProperty(PropertyAddress(kAudioPlugInPropertyTranslateUIDToBox), type: AudioObjectID.self, qualifier: PropertyQualifier(&qualifierData))
		guard boxObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(boxObjectID) as! AudioBox)
	}

	/// Returns the clock devices provided by the plug-in
	/// - remark: This corresponds to the property `kAudioPlugInPropertyClockDeviceList`
	public func clockDeviceList() throws -> [AudioClockDevice] {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyClockDeviceList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioClockDevice }
	}

	/// Returns the audio clock device provided by the plug-in with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioPlugInPropertyTranslateUIDToClockDevice`
	/// - parameter uid: The desired clock device UID
	public func clockDevice(forUID uid: String) throws -> AudioClockDevice? {
		var qualifierData = uid as CFString
		let clockDeviceObjectID = try getProperty(PropertyAddress(kAudioPlugInPropertyTranslateUIDToClockDevice), type: AudioObjectID.self, qualifier: PropertyQualifier(&qualifierData))
		guard clockDeviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(clockDeviceObjectID) as! AudioClockDevice)
	}
}

extension AudioPlugIn {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: AudioObjectSelector<AudioPlugIn>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<AudioPlugIn>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<AudioPlugIn>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObjectSelector where T == AudioPlugIn {
	/// The property selector `kAudioPlugInCreateAggregateDevice`
//	public static let createAggregateDevice = Selector(kAudioPlugInCreateAggregateDevice)
	/// The property selector `kAudioPlugInDestroyAggregateDevice`
//	public static let destroyAggregateDevice = Selector(kAudioPlugInDestroyAggregateDevice)
	/// The property selector `kAudioPlugInPropertyBundleID`
	public static let bundleID = AudioObjectSelector(kAudioPlugInPropertyBundleID)
	/// The property selector `kAudioPlugInPropertyDeviceList`
	public static let deviceList = AudioObjectSelector(kAudioPlugInPropertyDeviceList)
	/// The property selector `kAudioPlugInPropertyTranslateUIDToDevice`
	public static let translateUIDToDevice = AudioObjectSelector(kAudioPlugInPropertyTranslateUIDToDevice)
	/// The property selector `kAudioPlugInPropertyBoxList`
	public static let boxList = AudioObjectSelector(kAudioPlugInPropertyBoxList)
	/// The property selector `kAudioPlugInPropertyTranslateUIDToBox`
	public static let translateUIDToBox = AudioObjectSelector(kAudioPlugInPropertyTranslateUIDToBox)
	/// The property selector `kAudioPlugInPropertyClockDeviceList`
	public static let clockDeviceList = AudioObjectSelector(kAudioPlugInPropertyClockDeviceList)
	/// The property selector `kAudioPlugInPropertyTranslateUIDToClockDevice`
	public static let translateUIDToClockDevice = AudioObjectSelector(kAudioPlugInPropertyTranslateUIDToClockDevice)
}
