/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension ClockDevice {
	/// Returns an array of available sample rates or `nil` on error
	public var availableSampleRates: [AudioValueRange]? {
		guard let sampleRates = __availableSampleRates else {
			return nil
		}
		return sampleRates.map { $0.audioValueRangeValue() }
	}
}
