/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension SFBAudioPlayer {
	public var positionAndTime: (position: SFBAudioPlayerPlaybackPosition, time: SFBAudioPlayerPlaybackTime) {
		var position = SFBAudioPlayerPlaybackPosition()
		var time = SFBAudioPlayerPlaybackTime()
		__getPlaybackPosition(&position, andTime: &time)
		return (position: position, time: time)
	}
}
