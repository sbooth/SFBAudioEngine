/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioProperties {
	/// The total number of audio frames
	var frameLength: AVAudioFramePosition? {
		__frameLength?.int64Value
	}
	/// The number of channels
	var channelCount: AVAudioChannelCount? {
		__channelCount?.uint32Value
	}

	/// The number of bits per channel
	var bitsPerChannel: Int? {
		__bitsPerChannel?.intValue
	}

	/// The sample rate in Hz
	var sampleRate: Double? {
		__sampleRate?.doubleValue
	}

	/// The duration in seconds
	var duration: TimeInterval? {
		__duration?.doubleValue
	}

	/// The audio bitrate in KiB/sec
	var bitrate: Double? {
		__bitrate?.doubleValue
	}
}
