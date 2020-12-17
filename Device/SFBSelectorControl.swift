/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SelectorControl {
	/// Returns the selected items or `nil` on error
	/// - note: This corresponds to `kAudioSelectorControlPropertyCurrentItem`
	public var currentItem: [UInt]? {
		guard let values = __currentItem else {
			return nil
		}
		return values.map { $0.uintValue }
	}

	/// Returns the available items or `nil` on error
	/// - note: This corresponds to `kAudioSelectorControlPropertyAvailableItems`
	public var availableItems: [UInt]? {
		guard let values = __availableItems else {
			return nil
		}
		return values.map { $0.uintValue }
	}
}
