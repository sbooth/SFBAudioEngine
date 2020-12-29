//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio stream object (`kAudioStreamClassID`)
public class AudioStream: AudioObject {
}

extension AudioStream {
	/// Returns `true` if the stream is active (`kAudioStreamPropertyIsActive`)
	public func isActive() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioStreamPropertyIsActive)) != 0
	}

	/// Returns `true` if `self` is an output stream (`kAudioStreamPropertyDirection`)
	public func direction() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioStreamPropertyDirection)) != 0
	}

	/// Returns the terminal type (`kAudioStreamPropertyTerminalType`)
	public func terminalType() throws -> TerminalType {
		return TerminalType(rawValue: try getProperty(PropertyAddress(kAudioStreamPropertyTerminalType)))
	}

	/// Returns the starting channel (`kAudioStreamPropertyStartingChannel`)
	public func startingChannel() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioStreamPropertyStartingChannel))
	}

	/// Returns the latency (`kAudioStreamPropertyLatency`)
	public func latency() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioStreamPropertyLatency))
	}

	/// Returns the virtual format (`kAudioStreamPropertyVirtualFormat`)
	public func virtualFormat() throws -> AudioStreamBasicDescription {
		return try getProperty(PropertyAddress(kAudioStreamPropertyVirtualFormat))
	}
	/// Sets the virtual format (`kAudioStreamPropertyVirtualFormat`)
	public func setVirtualFormat(_ value: AudioStreamBasicDescription) throws {
		return try setProperty(PropertyAddress(kAudioStreamPropertyVirtualFormat), to: value)
	}

	/// Returns the available virtual formats (`kAudioStreamPropertyAvailableVirtualFormats`)
	public func availableVirtualFormats() throws -> [AudioStreamRangedDescription] {
		return try getProperty(PropertyAddress(kAudioStreamPropertyAvailableVirtualFormats))
	}

	/// Returns the physical format (`kAudioStreamPropertyPhysicalFormat`)
	public func physicalFormat() throws -> AudioStreamBasicDescription {
		return try getProperty(PropertyAddress(kAudioStreamPropertyPhysicalFormat))
	}
	/// Sets the physical format (`kAudioStreamPropertyPhysicalFormat`)
	public func setPhysicalFormat(_ value: AudioStreamBasicDescription) throws {
		return try setProperty(PropertyAddress(kAudioStreamPropertyPhysicalFormat), to: value)
	}

	/// Returns the available physical formats (`kAudioStreamPropertyAvailablePhysicalFormats`)
	public func availablePhysicalFormats() throws -> [AudioStreamRangedDescription] {
		return try getProperty(PropertyAddress(kAudioStreamPropertyAvailablePhysicalFormats))
	}
}

extension AudioStream {
	/// A thin wrapper around a HAL audio stream terminal type
	public struct TerminalType: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
		/// Unknown
		public static let unknown 					= TerminalType(rawValue: kAudioStreamTerminalTypeUnknown)
		/// Line level
		public static let line 						= TerminalType(rawValue: kAudioStreamTerminalTypeLine)
		/// Digital audio interface
		public static let digitalAudioInterface 	= TerminalType(rawValue: kAudioStreamTerminalTypeDigitalAudioInterface)
		/// Spekaer
		public static let speaker 					= TerminalType(rawValue: kAudioStreamTerminalTypeSpeaker)
		/// Headphones
		public static let headphones 				= TerminalType(rawValue: kAudioStreamTerminalTypeHeadphones)
		/// LFE speaker
		public static let lfeSpeaker 				= TerminalType(rawValue: kAudioStreamTerminalTypeLFESpeaker)
		/// Telephone handset speaker
		public static let receiverSpeaker 			= TerminalType(rawValue: kAudioStreamTerminalTypeReceiverSpeaker)
		/// Microphone
		public static let microphone 				= TerminalType(rawValue: kAudioStreamTerminalTypeMicrophone)
		/// Headset microphone
		public static let headsetMicrophone 		= TerminalType(rawValue: kAudioStreamTerminalTypeHeadsetMicrophone)
		/// Telephone handset microphone
		public static let receiverMicrophone 		= TerminalType(rawValue: kAudioStreamTerminalTypeReceiverMicrophone)
		/// TTY
		public static let tty 						= TerminalType(rawValue: kAudioStreamTerminalTypeTTY)
		/// HDMI
		public static let hdmi 						= TerminalType(rawValue: kAudioStreamTerminalTypeHDMI)
		/// DisplayPort
		public static let displayPort 				= TerminalType(rawValue: kAudioStreamTerminalTypeDisplayPort)

		public let rawValue: UInt32

		public init(rawValue: UInt32) {
			self.rawValue = rawValue
		}

		public init(integerLiteral value: UInt32) {
			self.rawValue = value
		}

		public init(stringLiteral value: StringLiteralType) {
			self.rawValue = value.fourCC
		}
	}
}

extension AudioStream.TerminalType: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self.rawValue {
		case kAudioStreamTerminalTypeUnknown:					return "Unknown"
		case kAudioStreamTerminalTypeLine:						return "Line Level"
		case kAudioStreamTerminalTypeDigitalAudioInterface: 	return "Digital Audio Interface"
		case kAudioStreamTerminalTypeSpeaker:					return "Speaker"
		case kAudioStreamTerminalTypeHeadphones:				return "Headphones"
		case kAudioStreamTerminalTypeLFESpeaker:				return "LFE Speaker"
		case kAudioStreamTerminalTypeReceiverSpeaker:			return "Receiver Speaker"
		case kAudioStreamTerminalTypeMicrophone: 				return "Microphone"
		case kAudioStreamTerminalTypeHeadsetMicrophone:			return "Headset Microphone"
		case kAudioStreamTerminalTypeReceiverMicrophone:		return "Receiver Microphone"
		case kAudioStreamTerminalTypeTTY:						return "TTY"
		case kAudioStreamTerminalTypeHDMI:						return "HDMI"
		case kAudioStreamTerminalTypeDisplayPort:				return "DisplayPort"
		default: 												return "\(self.rawValue)"
		}
	}
}
