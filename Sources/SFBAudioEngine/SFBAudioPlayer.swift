//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
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
        let position = playbackPosition
        return position == .invalid ? nil : position
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
        let time = playbackTime
        return time == .invalid ? nil : time
    }

    /// Returns the playback position and time in the current decoder or `nil` if the current decoder is `nil`
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
