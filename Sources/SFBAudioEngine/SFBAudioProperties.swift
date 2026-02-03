//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import Foundation

extension AudioProperties {
    /// The total number of audio frames
    public var frameLength: AVAudioFramePosition? {
        __frameLength?.int64Value
    }
    /// The number of channels
    public var channelCount: AVAudioChannelCount? {
        __channelCount?.uint32Value
    }

    /// The audio bit depth
    public var bitDepth: Int? {
        __bitDepth?.intValue
    }

    /// The sample rate in Hz
    public var sampleRate: Double? {
        __sampleRate?.doubleValue
    }

    /// The duration in seconds
    public var duration: TimeInterval? {
        __duration?.doubleValue
    }

    /// The audio bitrate in KiB/sec
    public var bitrate: Double? {
        __bitrate?.doubleValue
    }
}
