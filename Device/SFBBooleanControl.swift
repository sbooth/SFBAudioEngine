/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension LevelControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioBooleanControlPropertyValue`
	func value() throws -> Bool {
		return try getProperty(.booleanControlValue) != 0
	}

	/// Sets the control's value
	/// - note: This corresponds to `kAudioBooleanControlPropertyValue`
	func setValue(_ value: Bool) throws {
		try setProperty(.booleanControlValue, UInt(value ? 1 : 0))
	}
}
