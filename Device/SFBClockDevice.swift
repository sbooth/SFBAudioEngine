/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension ClockDevice {
	/// Returns an array of available sample rates or `[]` on error
	public var availableSampleRates: [Double] {
		guard let sampleRates = __availableSampleRates else {
			return []
		}
		return sampleRates.map { $0.doubleValue }
	}
}
