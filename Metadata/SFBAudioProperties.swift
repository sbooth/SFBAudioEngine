/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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

	/// The number of bits per channel
	public var bitsPerChannel: Int? {
		__bitsPerChannel?.intValue
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
