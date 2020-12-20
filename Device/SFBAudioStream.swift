/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioStream {
	/// Returns `true` if the stream is active
	/// - note: This corresponds to `kAudioStreamPropertyIsActive`
	/// - throws: An error if the property could not be retrieved
	public func isActive() throws -> Bool {
		return try getProperty(.streamIsActive) != 0
	}

	/// Returns `true` if the stream is an output stream
	/// - note: This corresponds to `kAudioStreamPropertyDirection`
	/// - throws: An error if the property could not be retrieved
	public func isOutput() throws -> Bool {
		return try getProperty(.streamDirection) != 0
	}

	/// Returns the terminal type
	/// - note: This corresponds to `kAudioStreamPropertyTerminalType`
	/// - throws: An error if the property could not be retrieved
	public func terminalType() throws -> TerminalType {
		return TerminalType(rawValue: UInt32(try getProperty(.streamTerminalType) as UInt))!
	}

	/// Returns the starting channel
	/// - note: This corresponds to `kAudioStreamPropertyStartingChannel`
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func startingChannel() throws -> UInt {
		return try getProperty(.streamStartingChannel)
	}

	/// Returns the latency
	/// - note: This corresponds to `kAudioStreamPropertyLatency`
	/// - throws: An error if the property could not be retrieved
	public func latency() throws -> UInt {
		return try getProperty(.streamLatency)
	}

	/// Returns the virtual format
	/// - note: This corresponds to `kAudioStreamPropertyVirtualFormat`
	/// - throws: An error if the property could not be retrieved
	public func virtualFormat() throws -> AudioStreamBasicDescription {
		return try getProperty(.streamVirtualFormat)
	}

	/// Sets the virtual format
	/// - note: This corresponds to `kAudioStreamPropertyVirtualFormat`
	/// - parameter value: The desired virtual format
	/// - throws: An error if the property could not be set
	public func setVirtualFormat(_ value: AudioStreamBasicDescription) throws {
		return try setProperty(.streamVirtualFormat, value)
	}

	/// Returns the available virtual formats
	/// - note: This corresponds to `kAudioStreamPropertyAvailableVirtualFormats`
	/// - throws: An error if the property could not be retrieved
	public func availableVirtualFormats() throws -> [AudioStreamRangedDescription] {
		return try getProperty(.streamAvailableVirtualFormats)
	}

	/// Returns the physical format
	/// - note: This corresponds to `kAudioStreamPropertyPhysicalFormat`
	/// - throws: An error if the property could not be retrieved
	public func physicalFormat() throws -> AudioStreamBasicDescription {
		return try getProperty(.streamPhysicalFormat)
	}

	/// Sets the physical format
	/// - note: This corresponds to `kAudioStreamPropertyPhysicalFormat`
	/// - parameter value: The desired physical format
	/// - throws: An error if the property could not be set
	public func setPhysicalFormat(_ value: AudioStreamBasicDescription) throws {
		return try setProperty(.streamPhysicalFormat, value)
	}

	/// Returns the available physical formats
	/// - note: This corresponds to `kAudioStreamPropertyAvailablePhysicalFormats`
	/// - throws: An error if the property could not be retrieved
	public func availablePhysicalFormats() throws -> [AudioStreamRangedDescription] {
		return try getProperty(.streamAvailablePhysicalFormats)
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
