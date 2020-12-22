/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SelectorControl {
	/// Returns the selected items
	/// - note: This corresponds to `kAudioSelectorControlPropertyCurrentItem`
	/// - throws: An error if the property could not be retrieved
	public func currentItem() throws -> [UInt32] {
		return try getProperty(.selectorControlCurrentItem)
	}

	/// Returns the available items
	/// - note: This corresponds to `kAudioSelectorControlPropertyAvailableItems`
	/// - throws: An error if the property could not be retrieved
	public func availableItems() throws -> [UInt32] {
		return try getProperty(.selectorControlAvailableItems)
	}

	/// Returns the name of `itemID`
	/// - note: This corresponds to `kAudioSelectorControlPropertyItemName`
	/// - parameter itemID: The item's ID
	/// - throws: An error if the property could not be retrieved
	public func nameOfItem(_ itemID: UInt32) throws -> String {
		var qualifier = itemID
		return try __string(forProperty: .selectorControlItemName, in: .global, onElement: .master, qualifier: &qualifier, qualifierSize: UInt32(MemoryLayout<UInt32>.size))
	}

	/// Returns the kind of `itemID`
	/// - note: This corresponds to `kAudioSelectorControlPropertyItemKind`
	/// - parameter itemID: The item's ID
	/// - throws: An error if the property could not be retrieved
	public func kindOfItem(_ itemID: UInt32) throws -> String {
		var qualifier = itemID
		return try __string(forProperty: .selectorControlItemKind, in: .global, onElement: .master, qualifier: &qualifier, qualifierSize: UInt32(MemoryLayout<UInt32>.size))
	}
}
