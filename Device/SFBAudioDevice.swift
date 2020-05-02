/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioDevice {
	public var availableSampleRates: [Double] {
		let sampleRates = __availableSampleRates
		return sampleRates.map { $0.doubleValue }
	}
}
