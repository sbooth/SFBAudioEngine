/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioPlayer {
	public var positionAndTime: (position: PlaybackPosition, time: PlaybackTime) {
		var position = PlaybackPosition()
		var time = PlaybackTime()
		__getPlaybackPosition(&position, andTime: &time)
		return (position: position, time: time)
	}
}
