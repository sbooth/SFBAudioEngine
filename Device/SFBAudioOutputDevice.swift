/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioOutputDevice {
	public var preferredStereoChannels: (left: UInt32, right: UInt32)? {
		guard let preferredChannels = self.__preferredStereoChannels, preferredChannels.count == 2 else {
			return nil;
		}
		return (preferredChannels[0].uint32Value, preferredChannels[1].uint32Value)
	}
}
