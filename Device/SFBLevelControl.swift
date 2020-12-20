/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension LevelControl {
	/// Returns the control's scalar value
	/// - note: This corresponds to `kAudioLevelControlPropertyScalarValue`
	/// - throws: An error if the property could not be retrieved
	public func scalarValue() throws -> Float {
		return try getProperty(.levelControlScalarValue)
	}

	/// Sets the control's scalar value
	/// - note: This corresponds to `kAudioLevelControlPropertyScalarValue`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setScalarValue(_ value: Float) throws {
		try setProperty(.levelControlScalarValue, value)
	}

	/// Returns the control's decibel value
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelValue`
	/// - throws: An error if the property could not be retrieved
	public func decibelValue() throws -> Float {
		return try getProperty(.levelControlDecibelValue)
	}

	/// Sets the control's decibel value
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelValue`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setDecibelValue(_ value: Float) throws {
		try setProperty(.levelControlDecibelValue, value)
	}

	/// Returns the decibel range
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelRange`
	/// - throws: An error if the property could not be retrieved
	public func decibelRange() throws -> AudioValueRange {
		return try getProperty(.levelControlDecibelRange)
	}

	/// Converts `scalar` to decibels and returns the converted value
	/// - note: This corresponds to `kAudioLevelControlPropertyConvertScalarToDecibels`
	/// - parameter scalar: The value to convert
	/// - throws: An error if the property could not be set
	public func convertToDecibels(_ scalar: Float) throws -> Float {
		return try __convertToDecibels(fromScalar: scalar).floatValue
	}

	/// Converts `decibels` to scalar and returns the converted value
	/// - note: This corresponds to `kAudioLevelControlPropertyConvertDecibelsToScalar`
	/// - parameter decibels: The value to convert
	/// - throws: An error if the property could not be set
	public func convertToScalar(_ decibels: Float) throws -> Float {
		return try __convertToScalar(fromDecibels: decibels).floatValue
	}
}
