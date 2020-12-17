/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SelectorControl {
	/// Returns the selected items
	/// - note: This corresponds to `kAudioSelectorControlPropertyCurrentItem`
	public func currentItem() throws -> [UInt] {
		return try uintsForProperty(.selectorControlCurrentItem)
	}

	/// Returns the available items
	/// - note: This corresponds to `kAudioSelectorControlPropertyAvailableItems`
	public func availableItems() throws -> [UInt] {
		return try uintsForProperty(.selectorControlAvailableItems)
	}

	/// Returns the item's name
	/// - note: This corresponds to `kAudioSelectorControlPropertyItemName`
	public func itemName() throws -> String {
		return try stringForProperty(.selectorControlItemName)
	}

	/// Returns the item's kind
	/// - note: This corresponds to `kAudioSelectorControlPropertyItemKind`
	public func itemKind() throws -> String {
		return try stringForProperty(.selectorControlItemKind)
	}
}
