/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SliderControl {
	/// Returns the available values
	/// - note: This corresponds to `kAudioSliderControlPropertyRange`
	public var range: [UInt]? {
		guard let values = __range else {
			return nil
		}
		return values.map { $0.uintValue }
	}
}
