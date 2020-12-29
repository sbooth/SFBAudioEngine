//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio boolean control object (`kAudioBooleanControlClassID`)
public class BooleanControl: AudioControl {
}

extension BooleanControl {
	/// Returns the control's value (`kAudioBooleanControlPropertyValue`)
	public func value() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioBooleanControlPropertyValue)) != 0
	}
	/// Sets the control's value (`kAudioBooleanControlPropertyValue`)
	public func setValue(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioBooleanControlPropertyValue), to: value ? 1 : 0)
	}
}

// MARK: -

/// A HAL audio mute control object (`kAudioMuteControlClassID`)
public class MuteControl: BooleanControl {
}

/// A HAL audio solo control object (`kAudioSoloControlClassID`)
public class SoloControl: BooleanControl {
}

/// A HAL audio jack control object (`kAudioJackControlClassID`)
public class JackControl: BooleanControl {
}

/// A HAL audio LFE mute control object (`kAudioLFEMuteControlClassID`)
public class LFEMuteControl: BooleanControl {
}

/// A HAL audio phantom power control object (`kAudioPhantomPowerControlClassID`)
public class PhantomPowerControl: BooleanControl {
}

/// A HAL audio phase invert control object (`kAudioPhaseInvertControlClassID`)
public class PhaseInvertControl: BooleanControl {
}

/// A HAL audio clip light control object (`kAudioClipLightControlClassID`)
public class ClipLightControl: BooleanControl {
}

/// A HAL audio talkback control object (`kAudioTalkbackControlClassID`)
public class TalkbackControl: BooleanControl {
}

/// A HAL audio listenback control object (`kAudioListenbackControlClassID`)
public class ListenbackControl: BooleanControl {
}
