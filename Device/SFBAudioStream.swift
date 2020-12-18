/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioStream {
	/// Returns `true` if the stream is active
	/// - note: This corresponds to `kAudioStreamPropertyIsActive`
	/// - parameter element: The desired element
	/// - returns: `true` if the stream is active
	public func isActive(_ element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.streamIsActive, scope: .global, element: element) != 0
	}

	/// Returns `true` if the stream is an output stream
	/// - note: This corresponds to `kAudioStreamPropertyDirection`
	/// - parameter element: The desired element
	/// - returns: `true` if the stream is an output stream
	public func isOutput(_ element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.streamDirection, scope: .global, element: element) != 0
	}

	/// Returns the terminal type
	/// - note: This corresponds to `kAudioStreamPropertyTerminalType`
	/// - parameter element: The desired element
	/// - returns: The terminal type
	public func terminalType(_ element: PropertyElement = .master) throws -> TerminalType {
		return TerminalType(rawValue: UInt32(try getProperty(.streamTerminalType, element: element) as UInt))!
	}

	/// Returns the starting channel
	/// - note: This corresponds to `kAudioStreamPropertyStartingChannel`
	/// - parameter element: The desired element
	/// - returns: The starting channel
	public func startingChannel(_ element: PropertyElement = .master) throws -> UInt {
		return try getProperty(.streamStartingChannel, scope: .global, element: element)
	}

	/// Returns the latency
	/// - note: This corresponds to `kAudioStreamPropertyLatency`
	/// - parameter element: The desired element
	/// - returns: The latency
	public func latency(_ element: PropertyElement = .master) throws -> UInt {
		return try getProperty(.streamLatency, scope: .global, element: element)
	}

	/// Returns the virtual format
	/// - note: This corresponds to `kAudioStreamPropertyVirtualFormat`
	/// - parameter element: The desired element
	/// - returns: The virtual format
	public func virtualFormat(_ element: PropertyElement = .master) throws -> AudioStreamBasicDescription {
		return try getProperty(.streamVirtualFormat, scope: .global, element: element)
	}

	/// Returns the available virtual formats
	/// - note: This corresponds to `kAudioStreamPropertyAvailableVirtualFormats`
	/// - parameter element: The desired element
	/// - returns: The available virtual formats
	public func availableVirtualFormats(_ element: PropertyElement = .master) throws -> [AudioStreamRangedDescription] {
		return try getProperty(.streamAvailableVirtualFormats, scope: .global, element: element)
	}

	/// Returns the physical format
	/// - note: This corresponds to `kAudioStreamPropertyPhysicalFormat`
	/// - parameter element: The desired element
	/// - returns: The physical format
	public func physicalFormat(_ element: PropertyElement = .master) throws -> AudioStreamBasicDescription {
		return try getProperty(.streamPhysicalFormat, scope: .global, element: element)
	}

	/// Returns the available physical formats
	/// - note: This corresponds to `kAudioStreamPropertyAvailablePhysicalFormats`
	/// - parameter element: The desired element
	/// - returns: The available physical formats
	public func availablePhysicalFormats(_ element: PropertyElement = .master) throws -> [AudioStreamRangedDescription] {
		return try getProperty(.streamAvailablePhysicalFormats, scope: .global, element: element)
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
		@unknown default: 				return "UNKNOWN (\(self.rawValue))"
		}
	}
}
