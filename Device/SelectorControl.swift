//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio selector control object
/// - remark: This class correponds to objects with base class `kAudioSelectorControlClassID`
public class SelectorControl: AudioControl {
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
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyCurrentItem))
	}
	/// Sets the selected items
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyCurrentItem`
	public func setCurrentItem(_ value: [UInt32]) throws {
		try setProperty(PropertyAddress((kAudioSelectorControlPropertyCurrentItem)), to: value)
	}

	/// Returns the available items
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyAvailableItems`
	public func availableItems() throws -> [UInt32] {
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyAvailableItems))
	}

	/// Returns the name of `itemID`
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyItemName`
	public func nameOfItem(_ itemID: UInt32) throws -> String {
		var qualifier = itemID
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyItemName), qualifier: PropertyQualifier(&qualifier))
	}

	/// Returns the kind of `itemID`
	/// - remark: This corresponds to the property `kAudioSelectorControlPropertyItemKind`
	public func kindOfItem(_ itemID: UInt32) throws -> UInt32 {
		var qualifier = itemID
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyItemKind), qualifier: PropertyQualifier(&qualifier))
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
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)): '\(id.fourCC)' \"\(name)\" on SelectorControl 0x\(String(control.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)): '\(id.fourCC)' on SelectorControl 0x\(String(control.objectID, radix: 16, uppercase: false)))>"
		}
	}
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
