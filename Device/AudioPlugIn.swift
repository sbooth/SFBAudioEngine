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
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyPlugInList)).map { AudioObject.make($0) as! AudioPlugIn }
	}

	/// Returns an initialized `AudioPlugIn` with `bundleID` or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateBundleIDToPlugIn` on `kAudioObjectSystemObject`
	/// - parameter bundleID: The desired bundle ID
	public class func makePlugIn(forBundleID bundleID: String) throws -> AudioPlugIn? {
		var qualifier = bundleID as CFString
		let objectID: AudioObjectID = try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateBundleIDToPlugIn), qualifier: PropertyQualifier(&qualifier))
		guard objectID != kAudioObjectUnknown else {
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
	func createAggregateDevice(composition: [AnyHashable: Any]) throws -> AudioDevice {
		var qualifier = composition as CFDictionary
		return AudioObject.make(try getProperty(PropertyAddress(kAudioPlugInCreateAggregateDevice), qualifier: PropertyQualifier(&qualifier))) as! AudioDevice
	}

	/// Destroys an aggregate device
	/// - remark: This corresponds to the property `kAudioPlugInDestroyAggregateDevice`
	func destroyAggregateDevice(_ aggregateDevice: AudioDevice) throws {
		_ = try getProperty(PropertyAddress(kAudioPlugInDestroyAggregateDevice), initialValue: aggregateDevice.objectID)
	}

	/// Returns the plug-in's bundle ID
	/// - remark: This corresponds to the property `kAudioPlugInPropertyBundleID`
	public func bundleID() throws -> String {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyBundleID))
	}

	/// Returns the audio devices provided by the plug-in
	/// - remark: This corresponds to the property `kAudioPlugInPropertyDeviceList`
	public func deviceList() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyDeviceList)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the audio device provided by the plug-in with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioPlugInPropertyTranslateUIDToDevice`
	/// - parameter uid: The desired device UID
	public func device(forUID uid: String) throws -> AudioDevice? {
		var qualifierData = uid as CFString
		let deviceObjectID: AudioObjectID = try getProperty(PropertyAddress(kAudioPlugInPropertyTranslateUIDToDevice), qualifier: PropertyQualifier(&qualifierData))
		guard deviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(deviceObjectID) as! AudioDevice)
	}

	/// Returns the audio boxes provided by the plug-in
	/// - remark: This corresponds to the property `kAudioPlugInPropertyBoxList`
	public func boxList() throws -> [AudioBox] {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyBoxList)).map { AudioObject.make($0) as! AudioBox }
	}

	/// Returns the audio box provided by the plug-in with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioPlugInPropertyTranslateUIDToBox`
	/// - parameter uid: The desired box UID
	public func box(forUID uid: String) throws -> AudioBox? {
		var qualifierData = uid as CFString
		let boxObjectID: AudioObjectID = try getProperty(PropertyAddress(kAudioPlugInPropertyTranslateUIDToBox), qualifier: PropertyQualifier(&qualifierData))
		guard boxObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(boxObjectID) as! AudioBox)
	}

	/// Returns the clock devices provided by the plug-in
	/// - remark: This corresponds to the property `kAudioPlugInPropertyClockDeviceList`
	public func clockDeviceList() throws -> [AudioClockDevice] {
		return try getProperty(PropertyAddress(kAudioPlugInPropertyClockDeviceList)).map { AudioObject.make($0) as! AudioClockDevice }
	}

	/// Returns the audio clock device provided by the plug-in with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioPlugInPropertyTranslateUIDToClockDevice`
	/// - parameter uid: The desired clock device UID
	public func clockDevice(forUID uid: String) throws -> AudioClockDevice? {
		var qualifierData = uid as CFString
		let clockDeviceObjectID: AudioObjectID = try getProperty(PropertyAddress(kAudioPlugInPropertyTranslateUIDToClockDevice), qualifier: PropertyQualifier(&qualifierData))
		guard clockDeviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(clockDeviceObjectID) as! AudioClockDevice)
	}
}

extension AudioPlugIn {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: Selector<AudioPlugIn>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<AudioPlugIn>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<AudioPlugIn>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension Selector where T == AudioPlugIn {
	/// The property `kAudioPlugInCreateAggregateDevice`
//	public static let createAggregateDevice = Selector(kAudioPlugInCreateAggregateDevice)
	/// The property `kAudioPlugInDestroyAggregateDevice`
//	public static let destroyAggregateDevice = Selector(kAudioPlugInDestroyAggregateDevice)
	/// The property `kAudioPlugInPropertyBundleID`
	public static let bundleID = Selector(kAudioPlugInPropertyBundleID)
	/// The property `kAudioPlugInPropertyDeviceList`
	public static let deviceList = Selector(kAudioPlugInPropertyDeviceList)
	/// The property `kAudioPlugInPropertyTranslateUIDToDevice`
	public static let translateUIDToDevice = Selector(kAudioPlugInPropertyTranslateUIDToDevice)
	/// The property `kAudioPlugInPropertyBoxList`
	public static let boxList = Selector(kAudioPlugInPropertyBoxList)
	/// The property `kAudioPlugInPropertyTranslateUIDToBox`
	public static let translateUIDToBox = Selector(kAudioPlugInPropertyTranslateUIDToBox)
	/// The property `kAudioPlugInPropertyClockDeviceList`
	public static let clockDeviceList = Selector(kAudioPlugInPropertyClockDeviceList)
	/// The property `kAudioPlugInPropertyTranslateUIDToClockDevice`
	public static let translateUIDToClockDevice = Selector(kAudioPlugInPropertyTranslateUIDToClockDevice)
}
