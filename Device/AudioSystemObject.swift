//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// The HAL audio system object (`kAudioSystemObjectClassID`)
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
public class AudioSystemObject: AudioObject {
	/// The singleton audio system object
	public static var instance = AudioSystemObject(AudioObjectID(kAudioObjectSystemObject))
}

extension AudioSystemObject {
}
