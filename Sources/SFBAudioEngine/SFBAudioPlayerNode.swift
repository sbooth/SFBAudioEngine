//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension AudioPlayerNode {
	/// Returns the playback position and time in the current decoder or `nil` if the current decoder is `nil`
	public var playbackPositionAndTime: (position: PlaybackPosition, time: PlaybackTime) {
		var positionAndTime = (position: PlaybackPosition(), time: PlaybackTime())
		getPlaybackPosition(&positionAndTime.position, andTime: &positionAndTime.time)
		return positionAndTime
	}
}
