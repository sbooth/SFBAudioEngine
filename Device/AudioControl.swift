//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio control object (`kAudioControlClassID`)
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
public class AudioControl: AudioObject {
}

extension AudioControl {
	/// Returns the control's scope (`kAudioControlPropertyScope`)
	public func scope() throws -> PropertyScope {
		return PropertyScope(rawValue: try getProperty(PropertyAddress(kAudioControlPropertyScope)))
	}

	/// Returns the control's element (`kAudioControlPropertyElement`)
	public func element() throws -> PropertyElement {
		return PropertyElement(rawValue: try getProperty(PropertyAddress(kAudioControlPropertyElement)))
	}
}
