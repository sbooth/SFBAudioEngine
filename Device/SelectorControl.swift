//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio selector control object (`kAudioSelectorControlClassID`)
public class SelectorControl: AudioControl {
}

extension SelectorControl {
	/// Returns the selected items (`kAudioSelectorControlPropertyCurrentItem`)
	public func currentItem() throws -> [UInt32] {
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyCurrentItem))
	}
	/// Sets the selected items (`kAudioSelectorControlPropertyCurrentItem`)
	public func setCurrentItem(_ value: [UInt32]) throws {
		try setProperty(PropertyAddress((kAudioSelectorControlPropertyCurrentItem)), to: value)
	}

	/// Returns the available items (`kAudioSelectorControlPropertyAvailableItems`)
	public func availableItems() throws -> [UInt32] {
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyAvailableItems))
	}

	/// Returns the name of `itemID` (`kAudioSelectorControlPropertyItemName`)
	public func nameOfItem(_ itemID: UInt32) throws -> String {
		var qualifier = itemID
		return try getProperty(PropertyAddress(kAudioSelectorControlPropertyItemName), qualifier: PropertyQualifier(&qualifier))
	}

	/// Returns the kind of `itemID` (`kAudioSelectorControlPropertyItemKind`)
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
			return "<\(type(of: self)) '\(id.fourCC)' \"\(name)\" on SelectorControl 0x\(String(control.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)) '\(id.fourCC)' on SelectorControl 0x\(String(control.objectID, radix: 16, uppercase: false)))>"
		}
	}
}

// MARK: -

/// A HAL audio data source control (`kAudioDataSourceControlClassID`)
public class DataSourceControl: SelectorControl {
}

/// A HAL audio data destination control (`kAudioDataDestinationControlClassID`)
public class DataDestinationControl: SelectorControl {
}

/// A HAL audio clock source control (`kAudioClockSourceControlClassID`)
public class ClockSourceControl: SelectorControl {
}

/// A HAL audio line level control (`kAudioLineLevelControlClassID`)
public class LineLevelControl: SelectorControl {
}

/// A HAL audio high pass filter control (`kAudioHighPassFilterControlClassID`)
public class HighPassFilterControl: SelectorControl {
}
