/*
* Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension LevelControl {
	/// Returns the decibel range or `nil` on error
	/// - note: This corresponds to `kAudioLevelControlPropertyDecibelRange`
	public var decibelRange: AudioValueRange? {
		guard let value = __decibelRange else {
			return nil
		}
		return value.audioValueRangeValue()
	}
}
