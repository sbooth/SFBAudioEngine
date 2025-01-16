//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension AudioPlayerNode {
	public var playbackPosition: PlaybackPosition? {
		var position = PlaybackPosition()
		guard getPlaybackPosition(&position, andTime: nil) else {
			return nil
		}
		return position
	}

	/// Returns the playback time in the current decoder or `nil` if the current decoder is `nil`
	public var playbackTime: PlaybackTime? {
		var time = PlaybackTime()
		guard getPlaybackPosition(nil, andTime: &time) else {
			return nil
		}
		return time
	}

	/// Returns the playback position and time in the current decoder or `nil` if the current decoder is `nil`
	public var playbackPositionAndTime: (position: PlaybackPosition, time: PlaybackTime)? {
		var positionAndTime = (position: PlaybackPosition(), time: PlaybackTime())
		guard getPlaybackPosition(&positionAndTime.position, andTime: &positionAndTime.time) else {
			return nil
		}
		return positionAndTime
	}
}
extension AudioPlayerNode.PlaybackPosition {
	/// The current frame position or `nil` if unknown
	public var current: AVAudioFramePosition? {
		framePosition == unknownFramePosition ? nil : framePosition
	}
	/// The total number of frames or `nil` if unknown
	public var total: AVAudioFramePosition? {
		frameLength == unknownFrameLength ? nil : frameLength
	}

	/// Returns `current` as a fraction of `total`
	public var progress: Double? {
		guard let current, let total else {
			return nil
		}
		return Double(current) / Double(total)
	}

	/// Returns the frames remaining
	public var remaining: AVAudioFramePosition? {
		guard let current, let total else {
			return nil
		}
		return total - current
	}
}

extension AudioPlayerNode.PlaybackTime {
	/// The current time or `nil` if unknown
	public var current: TimeInterval? {
		currentTime == unknownTime ? nil : currentTime
	}
	/// The total time or `nil` if unknown
	public var total: TimeInterval? {
		totalTime == unknownTime ? nil : totalTime
	}

	/// Returns `current` as a fraction of `total`
	public var progress: Double? {
		guard currentTime != unknownTime, totalTime != unknownTime else {
			return nil
		}
		return currentTime / totalTime
	}

	/// Returns the time remaining
	public var remaining: TimeInterval? {
		guard currentTime != unknownTime, totalTime != unknownTime else {
			return nil
		}
		return totalTime - currentTime
	}
}
