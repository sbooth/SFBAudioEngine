/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SliderControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioSliderControlPropertyValue`
	/// - returns: The control's value
	/// - throws: An error if the property could not be retrieved
	public func value() throws -> UInt {
		return try getProperty(.sliderControlValue)
	}

	/// Sets the control's value
	/// - note: This corresponds to `kAudioBooleanControlPropertyValue`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setValue(_ value: UInt) throws {
		try setProperty(.sliderControlValue, value)
	}

	/// Returns the available control values
	/// - note: This corresponds to `kAudioSliderControlPropertyRange`
	/// - returns: The available control value
	/// - throws: An error if the property could not be retrieved
	public func range() throws -> [UInt] {
		return try getProperty(.sliderControlRange)
	}
}
