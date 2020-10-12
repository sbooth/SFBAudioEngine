/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioPlayer {
	/// Returns the playback position and time in the current decoder or `((UnknownFramePosition, UnknownFrameLength), (UnknownTime, UnknownTime))` if the current decoder is `nil`
	public var positionAndTime: (position: PlaybackPosition, time: PlaybackTime) {
		var position = PlaybackPosition()
		var time = PlaybackTime()
		__getPlaybackPosition(&position, andTime: &time)
		return (position: position, time: time)
	}
}

extension AudioPlayer.PlaybackState: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .playing:
			return ".playing"
		case .paused:
			return ".paused"
		case .stopped:
			return ".stopped"
		@unknown default:
			fatalError()
		}
	}
}
