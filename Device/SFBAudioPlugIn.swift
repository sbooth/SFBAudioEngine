/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioPlugIn {
	/// Returns the bundle ID
	/// - note: This corresponds to `kAudioPlugInPropertyBundleID`
	public func bundleID() throws -> String {
		return try getProperty(.plugInBundleID)
	}

	/// Returns the audio devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyDeviceList`
	public func devices() throws -> [AudioDevice] {
		return try getProperty(.plugInDeviceList) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the audio devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyBoxList`
	public func boxes() throws -> [AudioBox] {
		return try getProperty(.plugInBoxList) as [AudioObject] as! [AudioBox]
	}

	/// Returns the audio clock devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyClockDeviceList`
	public func clockDevices() throws -> [ClockDevice] {
		return try getProperty(.plugInClockDeviceList) as [AudioObject] as! [ClockDevice]
	}
}
