//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension AudioPlayer {
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
		var position = PlaybackPosition()
		guard getPlaybackPosition(&position, andTime: nil) else {
			return nil
		}
		return position
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
		var time = PlaybackTime()
		guard getPlaybackPosition(nil, andTime: &time) else {
			return nil
		}
		return time
	}

	/// Returns the playback position and time in the current decoder or `nil` if the current decoder is `nil`
	public var positionAndTime: (position: PlaybackPosition, time: PlaybackTime)? {
		var position = PlaybackPosition()
		var time = PlaybackTime()
		guard getPlaybackPosition(&position, andTime: &time) else {
			return nil
		}
		return (position: position, time: time)
	}
}

extension AudioPlayer.PlaybackState: @retroactive CustomDebugStringConvertible {
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
