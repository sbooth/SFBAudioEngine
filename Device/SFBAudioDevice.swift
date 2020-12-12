/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioDevice {
	/// Returns an array of available sample rates or `[]` on error
	public var availableSampleRates: [Double] {
		guard let sampleRates = __availableSampleRates else {
			return []
		}
		return sampleRates.map { $0.doubleValue }
	}

	/// Returns the preferred stereo channels for the device
	/// - note: This is the property `{ kAudioDevicePropertyPreferredChannelsForStereo, scope, kAudioObjectPropertyElementMaster }`
	/// - parameter scope: The desired scope
	/// - returns: The preferred stereo channels for the device
	public func preferredStereoChannels(_ scope: AudioObjectPropertyScope) -> (left: UInt32, right: UInt32)? {
		guard let preferredChannels = self.__preferredStereoChannels(inScope: scope), preferredChannels.count == 2 else {
			return nil;
		}
		return (preferredChannels[0].uint32Value, preferredChannels[1].uint32Value)
	}
}
