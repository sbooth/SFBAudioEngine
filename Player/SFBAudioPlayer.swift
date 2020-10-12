/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioPlayer {
	public typealias PlaybackPosition = AudioPlayerNode.PlaybackPosition
	public typealias PlaybackTime = AudioPlayerNode.PlaybackTime

	/// Returns the playback position in the current decoder or `nil` if the current decoder is `nil`
	public var playbackPosition: PlaybackPosition? {
		var position = SFBAudioPlayerPlaybackPosition()
		guard __getPlaybackPosition(&position, andTime: nil) else {
			return nil
		}
		return PlaybackPosition(position)
	}

	/// Returns the playback time in the current decoder or `nil` if the current decoder is `nil`
	public var playbackTime: PlaybackTime? {
		var time = SFBAudioPlayerPlaybackTime()
		guard __getPlaybackPosition(nil, andTime: &time) else {
			return nil
		}
		return PlaybackTime(time)
	}

	/// Returns the playback position and time in the current decoder or `nil` if the current decoder is `nil`
	public var positionAndTime: (position: PlaybackPosition, time: PlaybackTime)? {
		var position = SFBAudioPlayerPlaybackPosition()
		var time = SFBAudioPlayerPlaybackTime()
		guard __getPlaybackPosition(&position, andTime: &time) else {
			return nil
		}
		return (position: PlaybackPosition(position), time: PlaybackTime(time))
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
