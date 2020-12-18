/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension StereoPanControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioStereoPanControlPropertyValue`
	public func value() throws -> Float {
		return try getProperty(.stereoPanControlValue)
	}

	/// Returns the control's panning channels
	/// - note: This corresponds to `kAudioStereoPanControlPropertyPanningChannels`
	public func panningChannels() throws -> [UInt] {
		return try getProperty(.stereoPanControlPanningChannels)
	}
}
