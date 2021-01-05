//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio stereo pan control object
/// - remark: This class correponds to objects with base class `kAudioStereoPanControlClassID`
public class StereoPanControl: AudioControl {
	public override var debugDescription: String {
		do {
			let panningChannels = try self.panningChannels()
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), (\(try scope()), \(try element())), \(try value()), (\(panningChannels.0), \(panningChannels.1))>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension StereoPanControl {
	/// Returns the control's value
	/// - remark: This corresponds to the property `kAudioStereoPanControlPropertyValue`
	public func value() throws -> Float {
		return try getProperty(PropertyAddress(kAudioStereoPanControlPropertyValue), type: Float.self)
	}
	/// Sets the control's value
	/// - remark: This corresponds to the property `kAudioStereoPanControlPropertyValue`
	public func setValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioStereoPanControlPropertyValue), to: value)
	}

	/// Returns the control's panning channels
	/// - remark: This corresponds to the property `kAudioStereoPanControlPropertyPanningChannels`
	public func panningChannels() throws -> (PropertyElement, PropertyElement) {
		let channels = try getProperty(PropertyAddress(kAudioStereoPanControlPropertyPanningChannels), elementType: UInt32.self)
		precondition(channels.count == 2)
		return (PropertyElement(channels[0]), PropertyElement(channels[1]))
	}
	/// Sets the control's panning channels
	/// - remark: This corresponds to the property `kAudioStereoPanControlPropertyPanningChannels`
	public func setPanningChannels(_ value: (PropertyElement, PropertyElement)) throws {
		try setProperty(PropertyAddress(kAudioStereoPanControlPropertyPanningChannels), to: [value.0.rawValue, value.1.rawValue])
	}
}

extension StereoPanControl {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: AudioObjectSelector<StereoPanControl>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<StereoPanControl>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<StereoPanControl>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObjectSelector where T == StereoPanControl {
	/// The property selector `kAudioStereoPanControlPropertyValue`
	public static let value = AudioObjectSelector(kAudioStereoPanControlPropertyValue)
	/// The property selector `kAudioStereoPanControlPropertyPanningChannels`
	public static let panningChannels = AudioObjectSelector(kAudioStereoPanControlPropertyPanningChannels)
}
