/*
* Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioStream {
	/// Returns the virtual format or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyVirtualFormat`
	/// - parameter element: The desired element
	/// - returns: The virtual format
	public func virtualFormat(_ element: PropertyElement = .master) -> AudioStreamBasicDescription? {
		var format = AudioStreamBasicDescription()
		guard __getVirtualFormat(&format, onElement: element) else {
			return nil
		}
		return format
	}

	/// Returns the physical format or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyPhysicalFormat`
	/// - parameter element: The desired element
	/// - returns: The physical format
	public func physicalFormat(_ element: PropertyElement = .master) -> AudioStreamBasicDescription? {
		var format = AudioStreamBasicDescription()
		guard __getPhysicalFormat(&format, onElement: element) else {
			return nil
		}
		return format
	}
}

extension AudioStream.TerminalType: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self {
		case .unknown:					return ".unknown"
		case .line:						return ".line"
		case .digitalAudioInterface: 	return ".digitalAudioInterface"
		case .speaker:					return ".speaker"
		case .headphones:				return ".headphones"
		case .lfeSpeaker:				return ".LFESpeaker"
		case .receiverSpeaker:			return ".receiverSpeaker"
		case .microphone: 				return ".microphone"
		case .headsetMicrophone:		return ".headsetMicrophone"
		case .receiverMicrophone:		return ".receiverMicrophone"
		case .TTY:						return ".TTY"
		case .HDMI:						return ".HDMI"
		case .displayPort:				return ".displayPort"
		@unknown default: 				return "UNKNOWN"
		}
	}
}
