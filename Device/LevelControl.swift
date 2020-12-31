//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio level control object
/// - remark: This class correponds to objects with base class `kAudioLevelControlClassID`
public class LevelControl: AudioControl {
	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), (\(try scope()), \(try element())), \(try scalarValue())>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension LevelControl {
	/// Returns the control's scalar value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyScalarValue`
	public func scalarValue() throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyScalarValue))
	}
	/// Sets the control's scalar value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyScalarValue`
	public func setScalarValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioLevelControlPropertyScalarValue), to: value)
	}

	/// Returns the control's decibel value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyDecibelValue`
	public func decibelValue() throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyDecibelValue))
	}
	/// Sets the control's decibel value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyDecibelValue`
	public func setDecibelValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioLevelControlPropertyDecibelValue), to: value)
	}

	/// Returns the decibel range
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyDecibelRange`
	public func decibelRange() throws -> ClosedRange<Float> {
		let value: AudioValueRange = try getProperty(PropertyAddress(kAudioLevelControlPropertyDecibelRange))
		return Float(value.mMinimum) ... Float(value.mMaximum)
	}

	/// Converts `scalar` to decibels and returns the converted value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyConvertScalarToDecibels`
	/// - parameter scalar: The value to convert
	public func convertToDecibels(fromScalar scalar: Float) throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyConvertScalarToDecibels), initialValue: scalar)
	}

	/// Converts `decibels` to scalar and returns the converted value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyConvertDecibelsToScalar`
	/// - parameter decibels: The value to convert
	public func convertToScalar(fromDecibels decibels: Float) throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyConvertDecibelsToScalar), initialValue: decibels)
	}
}

// MARK: -

/// A HAL audio volume control object
/// - remark: This class correponds to objects with base class `kAudioVolumeControlClassID`
public class VolumeControl: LevelControl {
}

/// A HAL audio LFE volume control object
/// - remark: This class correponds to objects with base class `kAudioLFEVolumeControlClassID`
public class LFEVolumeControl: LevelControl {
}
