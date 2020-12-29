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
		return try getProperty(AudioObjectProperty(kAudioSelectorControlPropertyCurrentItem))
	}
	/// Sets the selected items (`kAudioSelectorControlPropertyCurrentItem`)
	public func setCurrentItem(_ value: [UInt32]) throws {
		try setProperty(AudioObjectProperty((kAudioSelectorControlPropertyCurrentItem)), to: value)
	}

	/// Returns the available items (`kAudioSelectorControlPropertyAvailableItems`)
	public func availableItems() throws -> [UInt32] {
		return try getProperty(AudioObjectProperty(kAudioSelectorControlPropertyAvailableItems))
	}

	/// Returns the name of `itemID` (`kAudioSelectorControlPropertyItemName`)
	public func nameOfItem(_ itemID: UInt32) throws -> String {
		var qualifier = itemID
		return try getProperty(AudioObjectProperty(kAudioSelectorControlPropertyItemName), qualifier: PropertyQualifier(&qualifier))
	}

	/// Returns the kind of `itemID` (`kAudioSelectorControlPropertyItemKind`)
	public func kindOfItem(_ itemID: UInt32) throws -> String {
		var qualifier = itemID
		return try getProperty(AudioObjectProperty(kAudioSelectorControlPropertyItemKind), qualifier: PropertyQualifier(&qualifier))
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
