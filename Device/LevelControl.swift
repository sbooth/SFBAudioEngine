//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio level control object (`kAudioLevelControlClassID`)
public class LevelControl: AudioControl {
}

extension LevelControl {
	/// Returns the control's scalar value (`kAudioLevelControlPropertyScalarValue`)
	public func scalarValue() throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyScalarValue))
	}
	/// Sets the control's scalar value (`kAudioLevelControlPropertyScalarValue`)
	public func setScalarValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioLevelControlPropertyScalarValue), to: value)
	}

	/// Returns the control's decibel value (`kAudioLevelControlPropertyDecibelValue`)
	public func decibelValue() throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyDecibelValue))
	}
	/// Sets the control's decibel value (`kAudioLevelControlPropertyDecibelValue`)
	public func setDecibelValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioLevelControlPropertyDecibelValue), to: value)
	}

	/// Returns the decibel range (`kAudioLevelControlPropertyDecibelRange`)
	public func decibelRange() throws -> ClosedRange<Float64> {
		let range: AudioValueRange = try getProperty(PropertyAddress(kAudioLevelControlPropertyDecibelRange))
		return range.mMinimum ... range.mMaximum
	}

	/// Converts `scalar` to decibels and returns the converted value (`kAudioLevelControlPropertyConvertScalarToDecibels`)
	/// - parameter scalar: The value to convert
	public func convertToDecibels(_ scalar: Float) throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyConvertScalarToDecibels), initialValue: scalar)
	}

	/// Converts `decibels` to scalar and returns the converted value (`kAudioLevelControlPropertyConvertDecibelsToScalar`)
	/// - parameter decibels: The value to convert
	public func convertToScalar(_ decibels: Float) throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyConvertDecibelsToScalar), initialValue: decibels)
	}
}

// MARK: -

/// A HAL audio volume control object (`kAudioVolumeControlClassID`)
public class VolumeControl: LevelControl {
}

/// A HAL audio LFE volume control object (`kAudioLFEVolumeControlClassID`)
public class LFEVolumeControl: LevelControl {
}
