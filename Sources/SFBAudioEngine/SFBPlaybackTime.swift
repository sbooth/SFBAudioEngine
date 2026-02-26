//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import Foundation

extension PlaybackTime {
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
        guard currentTime != unknownTime && totalTime != unknownTime else {
            return nil
        }
        return currentTime / totalTime
    }

    /// Returns the time remaining
    public var remaining: TimeInterval? {
        guard currentTime != unknownTime && totalTime != unknownTime else {
            return nil
        }
        return totalTime - currentTime
    }
}

extension PlaybackTime: Equatable {
    public static func == (lhs: PlaybackTime, rhs: PlaybackTime) -> Bool {
        lhs.currentTime == rhs.currentTime && lhs.totalTime == rhs.totalTime
    }
}
