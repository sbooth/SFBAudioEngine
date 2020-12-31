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
		return try getProperty(PropertyAddress(kAudioStereoPanControlPropertyValue))
	}
	/// Sets the control's value
	/// - remark: This corresponds to the property `kAudioStereoPanControlPropertyValue`
	public func setValue(_ value: Float) throws {
		try setProperty(PropertyAddress(kAudioStereoPanControlPropertyValue), to: value)
	}

	/// Returns the control's panning channels
	/// - remark: This corresponds to the property `kAudioStereoPanControlPropertyPanningChannels`
	public func panningChannels() throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(kAudioStereoPanControlPropertyPanningChannels))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}
	/// Sets the control's panning channels
	/// - remark: This corresponds to the property `kAudioStereoPanControlPropertyPanningChannels`
	public func setPanningChannels(_ value: (UInt32, UInt32)) throws {
		try setProperty(PropertyAddress(kAudioStereoPanControlPropertyPanningChannels), to: [value.0, value.1])
	}
}
