/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SliderControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioSliderControlPropertyValue`
	public func value() throws -> Float {
		return try floatForProperty(.sliderControlValue)
	}
	
	/// Returns the available values
	/// - note: This corresponds to `kAudioSliderControlPropertyRange`
	public func range() throws -> [UInt] {
		return try uintsForProperty(.sliderControlRange)
	}
}
