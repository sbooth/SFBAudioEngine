//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import Foundation

extension AudioPlayer {
    /// Returns the frame position from the current decoder or `nil` if unknown or the current playback snapshot is invalid
    public var framePosition: AVAudioFramePosition? {
        let framePosition = __framePosition
        return framePosition == unknownFramePosition ? nil : framePosition
    }

    /// Returns the frame length from the current decoder or `nil` if unknown or the current playback snapshot is invalid
    public var frameLength: AVAudioFramePosition? {
        let frameLength = __frameLength
        return frameLength == unknownFrameLength ? nil : frameLength
    }

    /// Returns the playback position from the current decoder or `nil` if invalid or the current playback snapshot is invalid
    /// - note: Depending on the decoder's capabilities, the returned playback position may be partially valid.
    public var position: PlaybackPosition? {
        let position = playbackPosition
        return position == .invalid ? nil : position
    }

    /// Returns the current time from the current decoder or `nil` if unknown or the current playback snapshot is invalid
    public var currentTime: TimeInterval? {
        let currentTime = __currentTime
        return currentTime == unknownTime ? nil : currentTime
    }

    /// Returns the total time from the current decoder or `nil` if unknown or the current playback snapshot is invalid
    public var totalTime: TimeInterval? {
        let totalTime = __totalTime
        return totalTime == unknownTime ? nil : totalTime
    }

    /// Returns the playback time from the current decoder or `nil` if invalid or the current playback snapshot is invalid
    /// - note: Depending on the decoder's capabilities, the returned playback time may be partially valid.
    public var time: PlaybackTime? {
        let time = playbackTime
        return time == .invalid ? nil : time
    }

    /// Returns the playback position and time from the current decoder or `nil` if the current playback snapshot is invalid
    /// - note: Depending on the decoder's capabilities, the returned playback position and time may be partially valid.
    public var positionAndTime: (position: PlaybackPosition, time: PlaybackTime)? {
        var positionAndTime = (position: PlaybackPosition(), time: PlaybackTime())
        guard getPlaybackPosition(&positionAndTime.position, andTime: &positionAndTime.time) else {
            return nil
        }
        return positionAndTime
    }
}

extension AudioPlayer.PlaybackState: /*@retroactive*/ Swift.CustomDebugStringConvertible {
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
