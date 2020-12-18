/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioControl {
	/// Returns the control's scope
	/// - note: This corresponds to `kAudioControlPropertyScope`
	func scope() throws -> PropertyScope {
		return AudioObject.PropertyScope(rawValue: UInt32(try getProperty(.controlScope) as UInt))!
	}

	/// Returns the control's element
	/// - note: This corresponds to `kAudioControlPropertyElement`
	func element() throws -> PropertyElement {
		return AudioControl.PropertyElement(try getProperty(.controlElement) as UInt)
	}
}
