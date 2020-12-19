/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension LevelControl {
	/// Returns the control's scalar value
	/// - note: This corresponds to `kAudioLevelControlPropertyScalarValue`
	public func scalarValue() throws -> Float {
		return try getProperty(.levelControlScalarValue)
	}

	/// Sets the control's scalar value
	/// - note: This corresponds to `kAudioLevelControlPropertyScalarValue`
	public func setScalarValue(_ value: Float) throws {
		try setProperty(.levelControlScalarValue, value)
	}

	/// Returns the control's decibel value
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelValue`
	public func decibelValue() throws -> Float {
		return try getProperty(.levelControlDecibelValue)
	}

	/// Sets the control's decibel value
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelValue`
	public func setDecibelValue(_ value: Float) throws {
		try setProperty(.levelControlDecibelValue, value)
	}

	/// Returns the decibel range
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelRange`
	public func decibelRange() throws -> AudioValueRange {
		return try getProperty(.levelControlDecibelRange)
	}

	/// Converts `scalar` to decibels and returns the converted value
	/// - note: This corresponds to `kAudioLevelControlPropertyConvertScalarToDecibels`
	public func convertToDecibels(_ scalar: Float) throws -> Float {
		return try __convertToDecibels(fromScalar: scalar).floatValue
	}

	/// Converts `decibels` to scalar and returns the converted value
	/// - note: This corresponds to `kAudioLevelControlPropertyConvertDecibelsToScalar`
	public func convertToScalar(_ decibels: Float) throws -> Float {
		return try __convertToScalar(fromDecibels: decibels).floatValue
	}
}
