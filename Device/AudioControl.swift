//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio control object
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
/// - remark: This class correponds to objects with base class `kAudioControlClassID`
public class AudioControl: AudioObject {
	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), (\(try scope()), \(try element()))>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension AudioControl {
	/// Returns the control's scope
	/// - remark: This corresponds to the property `kAudioControlPropertyScope`
	public func scope() throws -> PropertyScope {
		return PropertyScope(try getProperty(PropertyAddress(kAudioControlPropertyScope)))
	}

	/// Returns the control's element
	/// - remark: This corresponds to the property `kAudioControlPropertyElement`
	public func element() throws -> PropertyElement {
		return PropertyElement(try getProperty(PropertyAddress(kAudioControlPropertyElement)))
	}
}
