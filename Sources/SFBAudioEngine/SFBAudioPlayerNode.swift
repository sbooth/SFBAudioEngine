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

extension AudioPlayerNode.PlaybackPosition {
	/// The invalid playback position
	public static let invalid = AudioPlayerNode.PlaybackPosition(framePosition: unknownFramePosition, frameLength: unknownFrameLength)

	/// Returns `true` if the current frame position and total number of frames are valid
	public var isValid: Bool {
		framePosition != unknownFramePosition && frameLength != unknownFrameLength
	}
	/// Returns `true` if the current frame position is valid
	public var isFramePositionValid: Bool {
		framePosition != unknownFramePosition
	}
	/// Returns `true` if the total number of frames is valid
	public var isFrameLengthValid: Bool {
		frameLength != unknownFrameLength
	}

	/// The current frame position or `nil` if unknown
	public var current: AVAudioFramePosition? {
		isFramePositionValid ? framePosition : nil
	}
	/// The total number of frames or `nil` if unknown
	public var total: AVAudioFramePosition? {
		isFrameLengthValid ? frameLength : nil
	}

	/// Returns `current` as a fraction of `total`
	public var progress: Double? {
		guard isValid else {
			return nil
		}
		return Double(framePosition) / Double(frameLength)
	}

	/// Returns the frames remaining
	public var remaining: AVAudioFramePosition? {
		guard isValid else {
			return nil
		}
		return frameLength - framePosition
	}
}

extension AudioPlayerNode.PlaybackTime {
	/// The invalid playback time
	public static let invalid = AudioPlayerNode.PlaybackTime(currentTime: unknownTime, totalTime: unknownTime)

	/// Returns `true` if the current time and total time are valid
	public var isValid: Bool {
		currentTime != unknownTime && totalTime != unknownTime
	}
	/// Returns `true` if the current time is valid
	public var isCurrentTimeValid: Bool {
		currentTime != unknownTime
	}
	/// Returns `true` if the total time is valid
	public var isTotalTimeValid: Bool {
		totalTime != unknownTime
	}

	/// The current time or `nil` if unknown
	public var current: TimeInterval? {
		isCurrentTimeValid ? currentTime : nil
	}
	/// The total time or `nil` if unknown
	public var total: TimeInterval? {
		isTotalTimeValid ? totalTime : nil
	}

	/// Returns `current` as a fraction of `total`
	public var progress: Double? {
		guard isValid else {
			return nil
		}
		return currentTime / totalTime
	}

	/// Returns the time remaining
	public var remaining: TimeInterval? {
		guard isValid else {
			return nil
		}
		return totalTime - currentTime
	}
}
