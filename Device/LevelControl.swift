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
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyScalarValue), type: Float.self)
	}
	/// Sets the control's scalar value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyScalarValue`
	public func setScalarValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioLevelControlPropertyScalarValue), to: value)
	}

	/// Returns the control's decibel value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyDecibelValue`
	public func decibelValue() throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyDecibelValue), type: Float.self)
	}
	/// Sets the control's decibel value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyDecibelValue`
	public func setDecibelValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioLevelControlPropertyDecibelValue), to: value)
	}

	/// Returns the decibel range
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyDecibelRange`
	public func decibelRange() throws -> ClosedRange<Float> {
		let value = try getProperty(PropertyAddress(kAudioLevelControlPropertyDecibelRange), type: AudioValueRange.self)
		return Float(value.mMinimum) ... Float(value.mMaximum)
	}

	/// Converts `scalar` to decibels and returns the converted value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyConvertScalarToDecibels`
	/// - parameter scalar: The value to convert
	public func convertToDecibels(fromScalar scalar: Float) throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyConvertScalarToDecibels), type: Float.self, initialValue: scalar)
	}

	/// Converts `decibels` to scalar and returns the converted value
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyConvertDecibelsToScalar`
	/// - parameter decibels: The value to convert
	public func convertToScalar(fromDecibels decibels: Float) throws -> Float {
		return try getProperty(PropertyAddress(kAudioLevelControlPropertyConvertDecibelsToScalar), type: Float.self, initialValue: decibels)
	}

	/// Returns the decibels to scalar transfer function
	/// - remark: This corresponds to the property `kAudioLevelControlPropertyDecibelsToScalarTransferFunction`
	public func decibelsToScalarTransferFunction() throws -> AudioLevelControlTransferFunction {
		return AudioLevelControlTransferFunction(rawValue: try getProperty(PropertyAddress(kAudioLevelControlPropertyDecibelsToScalarTransferFunction), type: UInt32.self))!
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
	/// The property selector `kAudioLevelControlPropertyScalarValue`
	public static let scalarValue = Selector(kAudioLevelControlPropertyScalarValue)
	/// The property selector `kAudioLevelControlPropertyDecibelValue`
	public static let decibelValue = Selector(kAudioLevelControlPropertyDecibelValue)
	/// The property selector `kAudioLevelControlPropertyDecibelRange`
	public static let decibelRange = Selector(kAudioLevelControlPropertyDecibelRange)
	/// The property selector `kAudioLevelControlPropertyConvertScalarToDecibels`
	public static let scalarToDecibels = Selector(kAudioLevelControlPropertyConvertScalarToDecibels)
	/// The property selector `kAudioLevelControlPropertyConvertDecibelsToScalar`
	public static let decibelsToScalar = Selector(kAudioLevelControlPropertyConvertDecibelsToScalar)
	/// The property selector `kAudioLevelControlPropertyDecibelsToScalarTransferFunction`
	public static let decibelsToScalarTransferFunction = Selector(kAudioLevelControlPropertyDecibelsToScalarTransferFunction)
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

/// A HAL audio boot chime volume control object
/// - remark: This class correponds to objects with base class `kAudioBootChimeVolumeControlClassID`
public class BootChimeVolumeControl: LevelControl {
}

// MARK: -

extension AudioLevelControlTransferFunction: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .tranferFunctionLinear:	return "Linear"
		case .tranferFunction1Over3:	return "1/3"
		case .tranferFunction1Over2:	return "1/2"
		case .tranferFunction3Over4:	return "3/4"
		case .tranferFunction3Over2:	return "3/2"
		case .tranferFunction2Over1:	return "2/1"
		case .tranferFunction3Over1:	return "3/1"
		case .tranferFunction4Over1:	return "4/1"
		case .tranferFunction5Over1:	return "5/1"
		case .tranferFunction6Over1:	return "6/1"
		case .tranferFunction7Over1:	return "7/1"
		case .tranferFunction8Over1:	return "8/1"
		case .tranferFunction9Over1:	return "9/1"
		case .tranferFunction10Over1:	return "10/1"
		case .tranferFunction11Over1:	return "11/1"
		case .tranferFunction12Over1: 	return "12/1"
		default:						return "\(self.rawValue)"
		}
	}
}
