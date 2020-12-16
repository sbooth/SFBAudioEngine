/*
* Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioStream {
	/// Returns `true` if the stream is active or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyIsActive`
	/// - parameter element: The desired element
	/// - returns: `true` if the stream is active
	public func isActive(_ element: PropertyElement = .master) -> Bool? {
		guard let value = uintForProperty(.streamIsActive, scope: .global, element: element) else {
			return nil
		}
		return value != 0
	}

	/// Returns `true` if the stream is an output stream or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyDirection`
	/// - parameter element: The desired element
	/// - returns: `true` if the stream is an output stream
	public func isOutput(_ element: PropertyElement = .master) -> Bool? {
		guard let value = uintForProperty(.streamDirection, scope: .global, element: element) else {
			return nil
		}
		return value != 0
	}

	/// Returns the terminal type or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyTerminalType`
	/// - parameter element: The desired element
	/// - returns: The terminal type
	public func terminalType(_ element: PropertyElement = .master) -> TerminalType? {
		guard let value = uintForProperty(.streamTerminalType, scope: .global, element: element) else {
			return nil
		}
		return TerminalType(rawValue: UInt32(value))
	}

	/// Returns the starting channel or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyStartingChannel`
	/// - parameter element: The desired element
	/// - returns: The starting channel
	public func startingChannel(_ element: PropertyElement = .master) -> UInt? {
		return uintForProperty(.streamStartingChannel, scope: .global, element: element)
	}

	/// Returns the latency or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyLatency`
	/// - parameter element: The desired element
	/// - returns: The latency
	public func latency(_ element: PropertyElement = .master) -> UInt? {
		return uintForProperty(.streamLatency, scope: .global, element: element)
	}

	/// Returns the virtual format or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyVirtualFormat`
	/// - parameter element: The desired element
	/// - returns: The virtual format
	public func virtualFormat(_ element: PropertyElement = .master) -> AudioStreamBasicDescription? {
		guard let value = __virtualFormat(onElement: element) else {
			return nil
		}
		return value.audioStreamBasicDescriptionValue()
	}

	/// Returns the available virtual formats or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyAvailableVirtualFormats`
	/// - parameter element: The desired element
	/// - returns: The available virtual formats
	public func availableVirtualFormats(_ element: PropertyElement = .master) -> [AudioStreamRangedDescription]? {
		guard let values = __availableVirtualFormats(onElement: element) else {
			return nil
		}
		return values.map { $0.audioStreamRangedDescriptionValue() }
	}

	/// Returns the physical format or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyPhysicalFormat`
	/// - parameter element: The desired element
	/// - returns: The physical format
	public func physicalFormat(_ element: PropertyElement = .master) -> AudioStreamBasicDescription? {
		guard let value = __physicalFormat(onElement: element) else {
			return nil
		}
		return value.audioStreamBasicDescriptionValue()
	}

	/// Returns the available physical formats or `nil` on error
	/// - note: This corresponds to `kAudioStreamPropertyAvailablePhysicalFormats`
	/// - parameter element: The desired element
	/// - returns: The available physical formats
	public func availablePhysicalFormats(_ element: PropertyElement = .master) -> [AudioStreamRangedDescription]? {
		guard let values = __availablePhysicalFormats(onElement: element) else {
			return nil
		}
		return values.map { $0.audioStreamRangedDescriptionValue() }
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
