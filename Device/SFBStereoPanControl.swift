/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension StereoPanControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioStereoPanControlPropertyValue`
	/// - throws: An error if the property could not be retrieved
	public func value() throws -> Float {
		return try getProperty(.stereoPanControlValue)
	}

	/// Sets the control's value
	/// - note: This corresponds to `kAudioStereoPanControlPropertyValue`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setValue(_ value: Float) throws {
		try setProperty(.stereoPanControlValue, value)
	}

	/// Returns the control's panning channels
	/// - note: This corresponds to `kAudioStereoPanControlPropertyPanningChannels`
	/// - throws: An error if the property could not be retrieved
	public func panningChannels() throws -> [UInt] {
		return try getProperty(.stereoPanControlPanningChannels)
	}
}
