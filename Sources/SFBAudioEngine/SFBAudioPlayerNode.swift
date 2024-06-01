//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension AudioPlayerNode {
	/// Playback position information for `AudioPlayerNode`
	public struct PlaybackPosition {
		/// The current frame position or `nil` if unknown
		public let current: AVAudioFramePosition?
		/// The total number of frames or `nil` if unknown
		public let total: AVAudioFramePosition?
	}

	/// Returns the playback position in the current decoder or `nil` if the current decoder is `nil`
	public var playbackPosition: PlaybackPosition? {
		var position = SFBAudioPlayerNodePlaybackPosition()
		guard __getPlaybackPosition(&position, andTime: nil) else {
			return nil
		}
		return PlaybackPosition(position)
	}

	/// Playback time information for `AudioPlayerNode`
	public struct PlaybackTime {
		/// The current time or `nil` if unknown
		public let current: TimeInterval?
		/// The total time or `nil` if unknown
		public let total: TimeInterval?
	}

	/// Returns the playback time in the current decoder or `nil` if the current decoder is `nil`
	public var playbackTime: PlaybackTime? {
		var time = SFBAudioPlayerNodePlaybackTime()
		guard __getPlaybackPosition(nil, andTime: &time) else {
			return nil
		}
		return PlaybackTime(time)
	}

	/// Returns the playback position and time in the current decoder or `nil` if the current decoder is `nil`
	public var playbackPositionAndTime: (position: PlaybackPosition, time: PlaybackTime)? {
		var position = SFBAudioPlayerNodePlaybackPosition()
		var time = SFBAudioPlayerNodePlaybackTime()
		guard __getPlaybackPosition(&position, andTime: &time) else {
			return nil
		}
		return (position: PlaybackPosition(position), time: PlaybackTime(time))
	}
}

extension AudioPlayerNode.PlaybackPosition {
	/// Returns an initialized `AudioPlayerNode.PlaybackPosition` object from `position`
	init(_ position: SFBAudioPlayerNodePlaybackPosition) {
		self.current = position.framePosition == unknownFramePosition ? nil : position.framePosition
		self.total = position.frameLength == unknownFrameLength ? nil : position.frameLength
	}

	/// Returns `current` as a fraction of `total`
	public var progress: Double? {
		guard let current = self.current, let total = self.total else {
			return nil
		}
		return Double(current) / Double(total)
	}

	/// Returns the frames remaining
	public var remaining: AVAudioFramePosition? {
		guard let current = self.current, let total = self.total else {
			return nil
		}
		return total - current
	}
}

extension AudioPlayerNode.PlaybackTime {
	/// Returns an initialized `AudioPlayerNode.PlaybackTime` object from `time`
	init(_ time: SFBAudioPlayerNodePlaybackTime) {
		self.current = time.currentTime == unknownTime ? nil : time.currentTime
		self.total = time.totalTime == unknownTime ? nil : time.totalTime
	}

	/// Returns `current` as a fraction of `total`
	public var progress: Double? {
		guard let current = self.current, let total = self.total else {
			return nil
		}
		return Double(current) / Double(total)
	}

	/// Returns the time remaining
	public var remaining: TimeInterval? {
		guard let current = self.current, let total = self.total else {
			return nil
		}
		return total - current
	}
}
