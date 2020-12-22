/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SliderControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioSliderControlPropertyValue`
	/// - throws: An error if the property could not be retrieved
	public func value() throws -> UInt32 {
		return try getProperty(.sliderControlValue)
	}

	/// Returns the available control values
	/// - note: This corresponds to `kAudioSliderControlPropertyRange`
	/// - throws: An error if the property could not be retrieved
	public func range() throws -> [UInt32] {
		return try getProperty(.sliderControlRange)
	}
}
