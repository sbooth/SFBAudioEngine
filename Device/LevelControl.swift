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

extension LevelControl {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: Selector<LevelControl>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<LevelControl>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<LevelControl>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension Selector where T == LevelControl {
	/// The property `kAudioLevelControlPropertyScalarValue`
	public static let scalarValue = Selector(kAudioLevelControlPropertyScalarValue)
	/// The property `kAudioLevelControlPropertyDecibelValue`
	public static let decibelValue = Selector(kAudioLevelControlPropertyDecibelValue)
	/// The property `kAudioLevelControlPropertyDecibelRange`
	public static let decibelRange = Selector(kAudioLevelControlPropertyDecibelRange)
	/// The property `kAudioLevelControlPropertyConvertScalarToDecibels`
	public static let scalarToDecibels = Selector(kAudioLevelControlPropertyConvertScalarToDecibels)
	/// The property `kAudioLevelControlPropertyConvertDecibelsToScalar`
	public static let decibelsToScalar = Selector(kAudioLevelControlPropertyConvertDecibelsToScalar)
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
