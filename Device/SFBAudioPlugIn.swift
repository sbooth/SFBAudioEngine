/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioPlugIn {
	/// Returns the bundle ID
	/// - note: This corresponds to `kAudioPlugInPropertyBundleID`
	/// - returns: The bundle ID
	/// - throws: An error if the property could not be retrieved
	public func bundleID() throws -> String {
		return try getProperty(.plugInBundleID)
	}

	/// Returns the audio devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyDeviceList`
	/// - returns: The audio devices provided by the plug in
	/// - throws: An error if the property could not be retrieved
	public func devices() throws -> [AudioDevice] {
		return try getProperty(.plugInDeviceList) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the audio boxes provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyBoxList`
	/// - returns: The audio boxes provided by the plug in
	/// - throws: An error if the property could not be retrieved
	public func boxes() throws -> [AudioBox] {
		return try getProperty(.plugInBoxList) as [AudioObject] as! [AudioBox]
	}

	/// Returns the audio clock devices provided by the plug in
	/// - note: This corresponds to `kAudioPlugInPropertyClockDeviceList`
	/// - returns: The audio clock devices provided by the plug in
	/// - throws: An error if the property could not be retrieved
	public func clockDevices() throws -> [ClockDevice] {
		return try getProperty(.plugInClockDeviceList) as [AudioObject] as! [ClockDevice]
	}
}
