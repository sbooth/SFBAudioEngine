/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioPlayer {
	public typealias PlaybackPosition = AudioPlayerNode.PlaybackPosition
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

	/// Returns the output device for `AVAudioEngine.outputNode`
	@available(macOS 10.15, *)
	public var outputDevice: AudioDevice {
		AudioObject.make(outputDeviceID) as! AudioDevice
	}

	/// Sets the output device for `AVAudioEngine.outputNode`
	@available(macOS 10.15, *)
	public func setOutputDevice(_ value: AudioDevice) throws {
		try setOutputDeviceID(value.objectID)
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
