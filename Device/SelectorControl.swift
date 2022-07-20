//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import Foundation
import CoreAudio

/// A HAL audio selector control object
/// - remark: This class correponds to objects with base class `kAudioSelectorControlClassID`
public class SelectorControl: AudioControl {
	// A textual representation of this instance, suitable for debugging.
	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), (\(try scope()), \(try element())), [\(try currentItem().map({ "'\($0.fourCC)'" }).joined(separator: ", "))]>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension SelectorControl {
	/// Returns the selected items
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyCurrentItem`
	public func currentItem() throws -> [UInt32] {
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyCurrentItem), elementType: UInt32.self)
	}
	/// Sets the selected items
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyCurrentItem`
	public func setCurrentItem(_ value: [UInt32]) throws {
		try setProperty(PropertyAddress((kAudioSelectorControlPropertyCurrentItem)), to: value)
	}

	/// Returns the available items
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyAvailableItems`
	public func availableItems() throws -> [UInt32] {
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyAvailableItems), elementType: UInt32.self)
	}

	/// Returns the name of `itemID`
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyItemName`
	public func nameOfItem(_ itemID: UInt32) throws -> String {
		var qualifier = itemID
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyItemName), type: CFString.self, qualifier: PropertyQualifier(&qualifier)) as String
	}

	/// Returns the kind of `itemID`
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyItemKind`
	public func kindOfItem(_ itemID: UInt32) throws -> UInt32 {
		var qualifier = itemID
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyItemKind), type: UInt32.self, qualifier: PropertyQualifier(&qualifier))
	}
}

extension SelectorControl {
	/// An item in a selector control
	public struct Item {
		/// The owning selector control
		public let control: SelectorControl
		/// The item ID
		public let id: UInt32

		/// Returns the item name
		public func name() throws -> String {
			return try control.nameOfItem(id)
		}

		/// Returns the item kind
		public func kind() throws -> UInt32 {
			return try control.kindOfItem(id)
		}
	}
}

extension SelectorControl.Item: CustomDebugStringConvertible {
	// A textual representation of this instance, suitable for debugging.
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)): '\(id.fourCC)' \"\(name)\" on SelectorControl 0x\(String(control.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)): '\(id.fourCC)' on SelectorControl 0x\(String(control.objectID, radix: 16, uppercase: false)))>"
		}
	}
}

extension SelectorControl {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: AudioObjectSelector<SelectorControl>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<SelectorControl>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<SelectorControl>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObjectSelector where T == SelectorControl {
	/// The property selector `kAudioSelectorControlPropertyCurrentItem`
	public static let currentItem = AudioObjectSelector(kAudioSelectorControlPropertyCurrentItem)
	/// The property selector `kAudioSelectorControlPropertyAvailableItems`
	public static let availableItems = AudioObjectSelector(kAudioSelectorControlPropertyAvailableItems)
	/// The property selector `kAudioSelectorControlPropertyItemName`
	public static let itemName = AudioObjectSelector(kAudioSelectorControlPropertyItemName)
	/// The property selector `kAudioSelectorControlPropertyItemKind`
	public static let itemKind = AudioObjectSelector(kAudioSelectorControlPropertyItemKind)
}

// MARK: -

/// A HAL audio data source control
/// - remark: This class correponds to objects with base class `kAudioDataSourceControlClassID`
public class DataSourceControl: SelectorControl {
}

/// A HAL audio data destination control
/// - remark: This class correponds to objects with base class `kAudioDataDestinationControlClassID`
public class DataDestinationControl: SelectorControl {
}

/// A HAL audio clock source control
/// - remark: This class correponds to objects with base class `kAudioClockSourceControlClassID`
public class ClockSourceControl: SelectorControl {
}

/// A HAL audio line level control
/// - remark: This class correponds to objects with base class `kAudioLineLevelControlClassID`
public class LineLevelControl: SelectorControl {
}

/// A HAL audio high pass filter control
/// - remark: This class correponds to objects with base class `kAudioHighPassFilterControlClassID`
public class HighPassFilterControl: SelectorControl {
}
