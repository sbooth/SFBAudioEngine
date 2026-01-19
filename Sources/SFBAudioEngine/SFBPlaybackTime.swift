//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation

extension PlaybackTime {
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
