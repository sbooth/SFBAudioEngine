/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

/// Replay Gain gain and peak information
public struct ReplayGain {
	/// The replay gain in dB
	public let gain: Float
	/// The normalized peak sample
	public let peak: Float
}

extension ReplayGainAnalyzer {
	public class func analyzeAlbum(_ urls: [URL]) throws -> (ReplayGain, [URL: ReplayGain]) {
		var trackReplayGain = [URL: ReplayGain]()

		let analyzer = ReplayGainAnalyzer()
		for url in urls {
			trackReplayGain[url] = try analyzer.analyzeTrack(url)
		}

		return (try analyzer.albumReplayGain(), trackReplayGain)
	}

	/// Returns replay gain gain and peak information for `url`
	public func analyzeTrack(_ url: URL) throws -> ReplayGain {
		let replayGain = try __analyzeTrack(url)
		return ReplayGain(gain: replayGain[.gainKey]!.floatValue, peak: replayGain[.peakKey]!.floatValue)
	}

	public func albumReplayGain() throws -> ReplayGain {
		let replayGain = try __albumGainAndPeakSampleReturningError()
		return ReplayGain(gain: replayGain[.gainKey]!.floatValue, peak: replayGain[.peakKey]!.floatValue)
	}
}
