//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CSFBAudioEngine

extension AudioPlayer {
	/// Playback position information for `AudioPlayer`
	public typealias PlaybackPosition = AudioPlayerNode.PlaybackPosition
	/// Playback time information for `AudioPlayer`
	public typealias PlaybackTime = AudioPlayerNode.PlaybackTime

	/// Returns the frame position in the current decoder or `nil` if the current decoder is `nil`
	public var framePosition: AVAudioFramePosition? {
		let framePosition = __framePosition
		return framePosition == unknownFramePosition ? nil : framePosition
	}

	/// Returns the frame length of the current decoder or `nil` if the current decoder is `nil`
	public var frameLength: AVAudioFramePosition? {
		let frameLength = __frameLength
		return frameLength == unknownFrameLength ? nil : frameLength
	}

	/// Returns the playback position in the current decoder or `nil` if the current decoder is `nil`
	public var position: PlaybackPosition? {
		var position = SFBAudioPlayerPlaybackPosition()
		guard __getPlaybackPosition(&position, andTime: nil) else {
			return nil
		}
		return PlaybackPosition(position)
	}

	/// Returns the current time in the current decoder or `nil` if the current decoder is `nil`
	public var currentTime: TimeInterval? {
		let currentTime = __currentTime
		return currentTime == unknownTime ? nil : currentTime
	}

	/// Returns the total time of the current decoder or `nil` if the current decoder is `nil`
	public var totalTime: TimeInterval? {
		let totalTime = __totalTime
		return totalTime == unknownTime ? nil : totalTime
	}

	/// Returns the playback time in the current decoder or `nil` if the current decoder is `nil`
	public var time: PlaybackTime? {
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

extension AudioPlayer.PlaybackState: /*@retroactive*/ CustomDebugStringConvertible {
	// A textual representation of this instance, suitable for debugging.
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
