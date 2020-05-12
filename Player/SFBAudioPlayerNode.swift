/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioPlayerNode {
	/// Returns the playback position and time in the current decoder or `((-1, -1), (-1, -1))` if the current decoder is `nil`
	public var positionAndTime: (position: PlaybackPosition, time: PlaybackTime) {
		var position = PlaybackPosition()
		var time = PlaybackTime()
		__getPlaybackPosition(&position, andTime: &time)
		return (position: position, time: time)
	}
}
