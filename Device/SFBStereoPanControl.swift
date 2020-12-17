/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension StereoPanControl {
	/// Returns the control's panning channels
	/// - note: This corresponds to `kAudioStereoPanControlPropertyPanningChannels`
	public var panningChannels: [UInt]? {
		guard let values = __panningChannels else {
			return nil
		}
		return values.map { $0.uintValue }
	}
}
