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
    /// Calculates replay gain for a single track
    /// - parameter url: The URL to analyze
    /// - returns: The track's gain and peak information
    /// - throws: An `NSError` object if an error occurs
    public class func analyzeTrack(_ url: URL) throws -> ReplayGain {
        let analyzer = ReplayGainAnalyzer()
        return try analyzer.analyzeTrack(url)
    }

    /// Calculates replay gain for an album
    /// - parameter urls: The URLs to analyze
    /// - returns: The album and track gain and peak information
    /// - throws: An `NSError` object if an error occurs
    public class func analyzeAlbum(_ urls: [URL]) throws -> (ReplayGain, [URL: ReplayGain]) {
        let analyzer = ReplayGainAnalyzer()
        return try analyzer.analyzeAlbum(urls)
    }

    /// Calculates replay gain for a single track
    /// - parameter url: The URL to analyze
    /// - returns: The track's gain and peak information
    /// - throws: An `NSError` object if an error occurs
    public func analyzeTrack(_ url: URL) throws -> ReplayGain {
        let replayGain = try __analyzeTrack(url)
        return ReplayGain(gain: replayGain[.gain]!.floatValue, peak: replayGain[.peak]!.floatValue)
    }

    /// Calculates replay gain for an album
    /// - parameter urls: The URLs to analyze
    /// - returns: The album and track gain and peak information
    /// - throws: An `NSError` object if an error occurs
    public func analyzeAlbum(_ urls: [URL]) throws -> (ReplayGain, [URL: ReplayGain]) {
        let replayGain = try __analyzeAlbum(urls)
        let albumReplayGain = ReplayGain(gain: (replayGain[Key.gain] as! NSNumber).floatValue, peak: (replayGain[Key.peak] as! NSNumber).floatValue)

        var trackReplayGains: [URL: ReplayGain] = [:]
        for (key, value) in replayGain {
            if let url = key as? URL, let trackReplayGain = value as? NSDictionary {
                trackReplayGains[url] = ReplayGain(gain: (trackReplayGain[Key.gain] as! NSNumber).floatValue, peak: (trackReplayGain[Key.peak] as! NSNumber).floatValue)
            }
        }

        return (albumReplayGain, trackReplayGains)
    }
}
