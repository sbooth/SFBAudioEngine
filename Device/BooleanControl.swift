//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CoreAudio

/// A HAL audio boolean control object
/// - remark: This class correponds to objects with base class `kAudioBooleanControlClassID`
public class BooleanControl: AudioControl {
	// A textual representation of this instance, suitable for debugging.
	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), (\(try scope()), \(try element())), \(try value() ? "On" : "Off")>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension BooleanControl {
	/// Returns the control's value
	/// - remark: This corresponds to the property `kAudioBooleanControlPropertyValue`
	public func value() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBooleanControlPropertyValue), type: UInt32.self) != 0
	}
	/// Sets the control's value
	/// - remark: This corresponds to the property `kAudioBooleanControlPropertyValue`
	public func setValue(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioBooleanControlPropertyValue), to: UInt32(value ? 1 : 0))
	}
}

extension BooleanControl {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: AudioObjectSelector<BooleanControl>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<BooleanControl>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<BooleanControl>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObjectSelector where T == BooleanControl {
	/// The property selector `kAudioBooleanControlPropertyValue`
	public static let value = AudioObjectSelector(kAudioBooleanControlPropertyValue)
}

// MARK: -

/// A HAL audio mute control object
/// - remark: This class correponds to objects with base class `kAudioMuteControlClassID`
public class MuteControl: BooleanControl {
}

/// A HAL audio solo control object
/// - remark: This class correponds to objects with base class `kAudioSoloControlClassID`
public class SoloControl: BooleanControl {
}

/// A HAL audio jack control object
/// - remark: This class correponds to objects with base class `kAudioJackControlClassID`
public class JackControl: BooleanControl {
}

/// A HAL audio LFE mute control object
/// - remark: This class correponds to objects with base class `kAudioLFEMuteControlClassID`
public class LFEMuteControl: BooleanControl {
}

/// A HAL audio phantom power control object
/// - remark: This class correponds to objects with base class `kAudioPhantomPowerControlClassID`
public class PhantomPowerControl: BooleanControl {
}

/// A HAL audio phase invert control object
/// - remark: This class correponds to objects with base class `kAudioPhaseInvertControlClassID`
public class PhaseInvertControl: BooleanControl {
}

/// A HAL audio clip light control object
/// - remark: This class correponds to objects with base class `kAudioClipLightControlClassID`
public class ClipLightControl: BooleanControl {
}

/// A HAL audio talkback control object
/// - remark: This class correponds to objects with base class `kAudioTalkbackControlClassID`
public class TalkbackControl: BooleanControl {
}

/// A HAL audio listenback control object
/// - remark: This class correponds to objects with base class `kAudioListenbackControlClassID`
public class ListenbackControl: BooleanControl {
}
