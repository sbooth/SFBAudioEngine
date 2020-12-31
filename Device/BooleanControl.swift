//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio boolean control object
/// - remark: This class correponds to objects with base class `kAudioBooleanControlClassID`
public class BooleanControl: AudioControl {
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
		return try getProperty(PropertyAddress(kAudioBooleanControlPropertyValue)) as UInt32 != 0
	}
	/// Sets the control's value
	/// - remark: This corresponds to the property `kAudioBooleanControlPropertyValue`
	public func setValue(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioBooleanControlPropertyValue), to: UInt32(value ? 1 : 0))
	}
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
