/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension ClockDevice {
	/// Returns the clock device UID
	/// - note: This corresponds to `kAudioClockDevicePropertyDeviceUID`
	/// - throws: An error if the property could not be retrieved
	public func clockDeviceUID() throws -> String {
		return try getProperty(.clockDeviceUID)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioClockDevicePropertyTransportType`
	/// - throws: An error if the property could not be retrieved
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: UInt32(try getProperty(.clockDeviceTransportType) as UInt))!
	}

	/// Returns the clock domain
	/// - note: This corresponds to `kAudioClockDevicePropertyClockDomain`
	/// - throws: An error if the property could not be retrieved
	public func domain() throws -> UInt {
		return try getProperty(.clockDeviceClockDomain)
	}

	/// Returns `true` if the clock device is alive
	/// - note: This corresponds to `kAudioClockDevicePropertyDeviceIsAlive`
	/// - throws: An error if the property could not be retrieved
	public func isAlive() throws -> Bool {
		return try getProperty(.clockDeviceIsAlive) != 0
	}

	/// Returns `true` if the clock device is running
	/// - note: This corresponds to `kAudioClockDevicePropertyDeviceIsRunning`
	/// - throws: An error if the property could not be retrieved
	public func isRunning() throws -> Bool {
		return try getProperty(.clockDeviceIsRunning) != 0
	}

	/// Returns the latency
	/// - note: This corresponds to `kAudioClockDevicePropertyLatency`
	/// - throws: An error if the property could not be retrieved
	public func latency() throws -> UInt {
		return try getProperty(.clockDeviceLatency)
	}

	/// Returns the audio controls
	/// - note: This corresponds to `kAudioClockDevicePropertyControlList`
	/// - throws: An error if the property could not be retrieved
	public func controls() throws -> [AudioControl] {
		return try getProperty(.clockDeviceControlList) as [AudioObject] as! [AudioControl]
	}

	/// Returns the device sample rate
	/// - note: This corresponds to `kAudioClockDevicePropertyNominalSampleRate`
	/// - throws: An error if the property could not be retrieved
	public func sampleRate() throws -> Double {
		return try getProperty(.clockDeviceNominalSampleRate)
	}

	/// Returns the available sample rates
	/// - note: This corresponds to `kAudioClockDevicePropertyAvailableNominalSampleRates`
	/// - throws: An error if the property could not be retrieved
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try getProperty(.clockDeviceAvailableNominalSampleRates)
	}
}
