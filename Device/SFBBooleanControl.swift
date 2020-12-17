/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension LevelControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioBooleanControlPropertyValue`
	func value() throws -> Bool {
		return try uintForProperty(.booleanControlValue) != 0
	}
}