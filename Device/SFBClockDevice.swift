/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension ClockDevice {
	/// Returns the clock device UID
	/// - note: This corresponds to `kAudioClockDevicePropertyDeviceUID`
	func clockDeviceUID() throws -> String {
		return try stringForProperty(.clockDeviceDeviceUID)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioClockDevicePropertyTransportType`
	func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: UInt32(try uintForProperty(.clockDeviceTransportType)))!
	}

	/// Returns the domain
	/// - note: This corresponds to `kAudioClockDevicePropertyClockDomain`
	public func domain() throws -> UInt {
		return try uintForProperty(.clockDeviceNominalSampleRate)
	}

	/// Returns `true` if the clock device is alive
	/// - note: This corresponds to `kAudioClockDevicePropertyDeviceIsAlive`
	public func isAlive() throws -> Bool {
		return try uintForProperty(.clockDeviceDeviceIsAlive) != 0
	}

	/// Returns `true` if the clock device is running
	/// - note: This corresponds to `kAudioClockDevicePropertyDeviceIsRunning`
	public func isRunning() throws -> Bool {
		return try uintForProperty(.clockDeviceDeviceIsRunning) != 0
	}

	/// Returns the latency
	/// - note: This corresponds to `kAudioClockDevicePropertyLatency`
	public func latency() throws -> UInt {
		return try uintForProperty(.clockDeviceLatency)
	}

	/// Returns the audio controls
	/// - note: This corresponds to `kAudioClockDevicePropertyControlList`
	func controls() throws -> [AudioControl] {
		return try audioObjectsForProperty(.clockDeviceControlList) as! [AudioControl]
	}

	/// Returns the device sample rate
	/// - note: This corresponds to `kAudioClockDevicePropertyNominalSampleRate`
	public func sampleRate() throws -> Double {
		return try doubleForProperty(.clockDeviceNominalSampleRate)
	}

	/// Returns the available sample rates
	/// - note: This corresponds to `kAudioClockDevicePropertyAvailableNominalSampleRates`
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try audioValueRangesForProperty(.clockDeviceAvailableNominalSampleRates)
	}
}
