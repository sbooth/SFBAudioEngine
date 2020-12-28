//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio slider control object (`kAudioSliderControlClassID`)
public class SliderControl: AudioControl {
}

extension SliderControl {
	/// Returns the control's value (`kAudioSliderControlPropertyValue`)
	public func value() throws -> UInt32 {
		return try getProperty(AudioObjectProperty(kAudioSliderControlPropertyValue))
	}
	/// Sets the control's value (`kAudioSliderControlPropertyValue`)
	public func setValue(_ value: UInt32) throws {
		try setProperty(AudioObjectProperty(kAudioSliderControlPropertyValue), to: value)
	}

	/// Returns the available control values (`kAudioSliderControlPropertyRange`)
	public func range() throws -> [UInt32] {
		return try getProperty(AudioObjectProperty(kAudioSliderControlPropertyRange))
	}
}
