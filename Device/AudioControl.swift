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

extension AudioControl {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: Selector<AudioControl>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<AudioControl>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<AudioControl>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension Selector where T == AudioControl {
	/// The property selector `kAudioControlPropertyScope`
	public static let scope = Selector(kAudioControlPropertyScope)
	/// The property selector `kAudioControlPropertyElement`
	public static let element = Selector(kAudioControlPropertyElement)
}
