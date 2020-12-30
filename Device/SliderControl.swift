//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio slider control object
/// - remark: This class correponds to objects with base class `kAudioSliderControlClassID`
public class SliderControl: AudioControl {
}

extension SliderControl {
	/// Returns the control's value
	/// - remark: This corresponds to the property `kAudioSliderControlPropertyValue`
	public func value() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioSliderControlPropertyValue))
	}
	/// Sets the control's value
	/// - remark: This corresponds to the property `kAudioSliderControlPropertyValue`
	public func setValue(_ value: UInt32) throws {
		try setProperty(PropertyAddress(kAudioSliderControlPropertyValue), to: value)
	}

	/// Returns the available control values
	/// - remark: This corresponds to the property `kAudioSliderControlPropertyRange`
	public func range() throws -> [UInt32] {
		return try getProperty(PropertyAddress(kAudioSliderControlPropertyRange))
	}
}
