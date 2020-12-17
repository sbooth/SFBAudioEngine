/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension LevelControl {
	/// Returns the control's scalar value
	/// - note: This corresponds to `kAudioLevelControlPropertyScalarValue`
	func scalarValue() throws -> Float {
		return try floatForProperty(.levelControlScalarValue)
	}

	/// Returns the control's decibel value
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelValue`
	func decibelValue() throws -> Float {
		return try floatForProperty(.levelControlDecibelValue)
	}

	/// Returns the decibel range
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelRange`
	public func decibelRange() throws -> AudioValueRange {
		return try audioValueRangeForProperty(.levelControlDecibelRange)
	}
}
