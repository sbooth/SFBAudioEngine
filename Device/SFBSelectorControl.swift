/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension SelectorControl {
	/// Returns the selected items
	/// - note: This corresponds to `kAudioSelectorControlPropertyCurrentItem`
	/// - returns: The selected items
	/// - throws: An error if the property could not be retrieved
	public func currentItem() throws -> [UInt] {
		return try getProperty(.selectorControlCurrentItem)
	}

	/// Returns the available items
	/// - note: This corresponds to `kAudioSelectorControlPropertyAvailableItems`
	/// - returns: The available items
	/// - throws: An error if the property could not be retrieved
	public func availableItems() throws -> [UInt] {
		return try getProperty(.selectorControlAvailableItems)
	}

	/// Returns the name of `itemID`
	/// - note: This corresponds to `kAudioSelectorControlPropertyItemName`
	/// - parameter itemID: The item's ID
	/// - returns: The item's name
	/// - throws: An error if the property could not be retrieved
	public func nameOfItem(_ itemID: UInt) throws -> String {
		var qualifier: UInt32 = UInt32(itemID)
		return try __string(forProperty: .selectorControlItemName, in: .global, onElement: .master, qualifier: &qualifier, qualifierSize: UInt32(MemoryLayout<UInt32>.size))
	}

	/// Returns the kind of `itemID`
	/// - note: This corresponds to `kAudioSelectorControlPropertyItemKind`
	/// - parameter itemID: The item's ID
	/// - returns: The item's kind
	/// - throws: An error if the property could not be retrieved
	public func kindOfItem(_ itemID: UInt) throws -> String {
		var qualifier: UInt32 = UInt32(itemID)
		return try __string(forProperty: .selectorControlItemKind, in: .global, onElement: .master, qualifier: &qualifier, qualifierSize: UInt32(MemoryLayout<UInt32>.size))
	}
}
