//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio stream object
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`), a master element (`kAudioObjectPropertyElementMaster`), and an element for each channel
/// - remark: This class correponds to objects with base class `kAudioStreamClassID`
public class AudioStream: AudioObject {
	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), \(try isActive() ? "active" : "inactive"), \(try direction() ? "output" : "input"), starting channel = \(try startingChannel()), virtual format = \(try virtualFormat()), physical format = \(try physicalFormat())>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension AudioStream {
	/// Returns `true` if the stream is active
	/// - remark: This corresponds to the property `kAudioStreamPropertyIsActive`
	public func isActive() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioStreamPropertyIsActive), type: UInt32.self) != 0
	}

	/// Returns `true` if `self` is an output stream
	/// - remark: This corresponds to the property `kAudioStreamPropertyDirection`
	public func direction() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioStreamPropertyDirection), type: UInt32.self) == 0
	}

	/// Returns the terminal type
	/// - remark: This corresponds to the property `kAudioStreamPropertyTerminalType`
	public func terminalType() throws -> TerminalType {
		return TerminalType(rawValue: try getProperty(PropertyAddress(kAudioStreamPropertyTerminalType), type: UInt32.self))
	}

	/// Returns the starting channel
	/// - remark: This corresponds to the property `kAudioStreamPropertyStartingChannel`
	public func startingChannel() throws -> PropertyElement {
		return PropertyElement(try getProperty(PropertyAddress(kAudioStreamPropertyStartingChannel), type: UInt32.self))
	}

	/// Returns the latency
	/// - remark: This corresponds to the property `kAudioStreamPropertyLatency`
	public func latency() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioStreamPropertyLatency), type: UInt32.self)
	}

	/// Returns the virtual format
	/// - remark: This corresponds to the property `kAudioStreamPropertyVirtualFormat`
	public func virtualFormat() throws -> AudioStreamBasicDescription {
		return try getProperty(PropertyAddress(kAudioStreamPropertyVirtualFormat))
	}
	/// Sets the virtual format
	/// - remark: This corresponds to the property `kAudioStreamPropertyVirtualFormat`
	public func setVirtualFormat(_ value: AudioStreamBasicDescription) throws {
		return try setProperty(PropertyAddress(kAudioStreamPropertyVirtualFormat), to: value)
	}

	/// Returns the available virtual formats
	/// - remark: This corresponds to the property `kAudioStreamPropertyAvailableVirtualFormats`
	public func availableVirtualFormats() throws -> [(AudioStreamBasicDescription, ClosedRange<Double>)] {
		let value = try getProperty(PropertyAddress(kAudioStreamPropertyAvailableVirtualFormats), elementType: AudioStreamRangedDescription.self)
		return value.map { ($0.mFormat, $0.mSampleRateRange.mMinimum ... $0.mSampleRateRange.mMaximum) }
	}

	/// Returns the physical format
	/// - remark: This corresponds to the property `kAudioStreamPropertyPhysicalFormat`
	public func physicalFormat() throws -> AudioStreamBasicDescription {
		return try getProperty(PropertyAddress(kAudioStreamPropertyPhysicalFormat))
	}
	/// Sets the physical format
	/// - remark: This corresponds to the property `kAudioStreamPropertyPhysicalFormat`
	public func setPhysicalFormat(_ value: AudioStreamBasicDescription) throws {
		return try setProperty(PropertyAddress(kAudioStreamPropertyPhysicalFormat), to: value)
	}

	/// Returns the available physical formats
	/// - remark: This corresponds to the property `kAudioStreamPropertyAvailablePhysicalFormats`
	public func availablePhysicalFormats() throws -> [(AudioStreamBasicDescription, ClosedRange<Double>)] {
		let value = try getProperty(PropertyAddress(kAudioStreamPropertyAvailablePhysicalFormats), elementType: AudioStreamRangedDescription.self)
		return value.map { ($0.mFormat, $0.mSampleRateRange.mMinimum ... $0.mSampleRateRange.mMaximum) }
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

extension AudioStream {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: AudioObjectSelector<AudioStream>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<AudioStream>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<AudioStream>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObjectSelector where T == AudioStream {
	/// The property selector `kAudioStreamPropertyIsActive`
	public static let isActive = AudioObjectSelector(kAudioStreamPropertyIsActive)
	/// The property selector `kAudioStreamPropertyDirection`
	public static let direction = AudioObjectSelector(kAudioStreamPropertyDirection)
	/// The property selector `kAudioStreamPropertyTerminalType`
	public static let terminalType = AudioObjectSelector(kAudioStreamPropertyTerminalType)
	/// The property selector `kAudioStreamPropertyStartingChannel`
	public static let startingChannel = AudioObjectSelector(kAudioStreamPropertyStartingChannel)
	/// The property selector `kAudioStreamPropertyLatency`
	public static let latency = AudioObjectSelector(kAudioStreamPropertyLatency)
	/// The property selector `kAudioStreamPropertyVirtualFormat`
	public static let virtualFormat = AudioObjectSelector(kAudioStreamPropertyVirtualFormat)
	/// The property selector `kAudioStreamPropertyAvailableVirtualFormats`
	public static let availableVirtualFormats = AudioObjectSelector(kAudioStreamPropertyAvailableVirtualFormats)
	/// The property selector `kAudioStreamPropertyPhysicalFormat`
	public static let physicalFormat = AudioObjectSelector(kAudioStreamPropertyPhysicalFormat)
	/// The property selector `kAudioStreamPropertyAvailablePhysicalFormats`
	public static let availablePhysicalFormats = AudioObjectSelector(kAudioStreamPropertyAvailablePhysicalFormats)
}
