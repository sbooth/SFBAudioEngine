/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioPlugIn {
	/// Returns the bundle ID
	/// - note: This corresponds to `kAudioPlugInPropertyBundleID`
	func bundleID() throws -> String {
		return try stringForProperty(.plugInBundleID)
	}

	/// Returns the audio devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyDeviceList`
	func devices() throws -> [AudioDevice] {
		return try audioObjectsForProperty(.plugInDeviceList) as! [AudioDevice]
	}

	/// Returns the audio devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyBoxList`
	func boxes() throws -> [AudioBox] {
		return try audioObjectsForProperty(.plugInBoxList) as! [AudioBox]
	}

	/// Returns the audio clock devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyClockDeviceList`
	func clockDevices() throws -> [ClockDevice] {
		return try audioObjectsForProperty(.plugInClockDeviceList) as! [ClockDevice]
	}
}
