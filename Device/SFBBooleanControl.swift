/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension LevelControl {
	/// Returns the control's value
	/// - note: This corresponds to `kAudioBooleanControlPropertyValue`
	/// - throws: An error if the property could not be retrieved
	public func value() throws -> Bool {
		return try getProperty(.booleanControlValue) != 0
	}
}
