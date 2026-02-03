//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import Foundation

/// Replay Gain gain and peak information
public struct ReplayGain {
    /// The replay gain in dB
    public let gain: Float
    /// The  peak sample normalized to [-1, 1)
    public let peak: Float
}

extension ReplayGainAnalyzer {
    /// Analyzes the given album's replay gain
    /// - parameter urls: The URLs to analyze
    /// - returns: The album's gain and peak information keyed by URL
    /// - throws: An `NSError` object if an error occurs
    public class func analyzeAlbum(_ urls: [URL]) throws -> (ReplayGain, [URL: ReplayGain]) {
        var trackReplayGain = [URL: ReplayGain]()

        let analyzer = ReplayGainAnalyzer()
        for url in urls {
            trackReplayGain[url] = try analyzer.analyzeTrack(url)
        }

        return (try analyzer.albumReplayGain(), trackReplayGain)
    }

    /// Returns replay gain gain and normalized peak information for `url`
    public func analyzeTrack(_ url: URL) throws -> ReplayGain {
        let replayGain = try __analyzeTrack(url)
        return ReplayGain(gain: replayGain[.gainKey]!.floatValue, peak: replayGain[.peakKey]!.floatValue)
    }

    /// Returns replay gain gain and normalized peak information for the album
    public func albumReplayGain() throws -> ReplayGain {
        let replayGain = try __albumGainAndPeakSampleReturningError()
        return ReplayGain(gain: replayGain[.gainKey]!.floatValue, peak: replayGain[.peakKey]!.floatValue)
    }
}
