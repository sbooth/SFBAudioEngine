/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioDevice {
	/// Returns `true` if this device is a private aggregate device
	/// - note: An aggregate device is private if `kAudioAggregateDeviceIsPrivateKey` in the composition dictionary is true
	public func isPrivateAggregate(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		if let aggregate = self as? AggregateDevice, try aggregate.isPrivate(scope, element: element) {
			return true;
		}
		return false
	}

	// MARK: - Device Base Properties
	
	/// Returns the configuration application
	/// - note: This corresponds to `kAudioDevicePropertyConfigurationApplication`
	public func configurationApplication(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> String {
		return try getProperty(.deviceConfigurationApplication, scope: scope, element: element)
	}

	/// Returns the device UID
	/// - note: This corresponds to `kAudioDevicePropertyDeviceUID`
	public func deviceUID(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> String {
		return try getProperty(.deviceUID, scope: scope, element: element)
	}

	/// Returns the model UID
	/// - note: This corresponds to `kAudioDevicePropertyModelUID`
	public func modelUID(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> String {
		return try getProperty(.deviceModelUID, scope: scope, element: element)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioDevicePropertyTransportType`
	public func transportType(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> TransportType {
		return TransportType(rawValue: UInt32(try getProperty(.deviceTransportType, scope: scope, element: element) as UInt))!
	}

	/// Returns related audio devices
	/// - note: This corresponds to `kAudioDevicePropertyRelatedDevices`
	public func relatedDevices(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioDevice] {
		return try getProperty(.deviceRelatedDevices, scope: scope, element: element) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the clock domain
	/// - note: This corresponds to `kAudioClockDevicePropertyClockDomain`
	public func clockDomain(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt {
		return try getProperty(.deviceClockDomain, scope: scope, element: element)
	}

	/// Returns `true` if the device is alive
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsAlive`
	public func isAlive(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceIsAlive, scope: scope, element: element) != 0
	}

	/// Returns `true` if the device is running
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsRunning`
	public func isRunning(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceIsRunning, scope: scope, element: element) != 0
	}

	/// Returns `true` if the device can be the default device
	/// - note: This corresponds to `kAudioDevicePropertyDeviceCanBeDefaultDevice`
	public func canBeDefault(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceCanBeDefaultDevice, scope: scope, element: element) != 0
	}

	/// Returns `true` if the device can be the system default device
	/// - note: This corresponds to `kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`
	public func canBeSystemDefault(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceCanBeDefaultSystemDevice, scope: scope, element: element) != 0
	}

	/// Returns the latency
	/// - note: This corresponds to `kAudioDevicePropertyLatency`
	public func latency(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt {
		return try getProperty(.deviceLatency, scope: scope, element: element)
	}

	/// Returns the device's streams
	/// - note: This corresponds to `kAudioDevicePropertyStreams`
	public func streams(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioStream] {
		return try getProperty(.deviceStreams, scope: scope, element: element) as [AudioObject] as! [AudioStream]
	}

	/// Returns the device's audio controls
	/// - note: This corresponds to `kAudioObjectPropertyControlList`
	public func controls(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioControl] {
		return try getProperty(.controlList, scope: scope, element: element) as [AudioObject] as! [AudioControl]
	}

	/// Returns the safety offset
	/// - note: This corresponds to `kAudioDevicePropertySafetyOffset`
	public func safetyOffset(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> UInt {
		return try getProperty(.deviceSafetyOffset, scope: scope, element: element)
	}

	/// Returns the sample rate
	/// - note: This corresponds to `kAudioDevicePropertyNominalSampleRate`
	public func sampleRate(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Double {
		return try getProperty(.deviceNominalSampleRate, scope: scope, element: element)
	}

	/// Sets the sample rate
	/// - note: This corresponds to `kAudioDevicePropertyNominalSampleRate`
	public func setSampleRate(_ value: Double, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try setProperty(.deviceNominalSampleRate, value, scope: scope, element: element)
	}

	/// Returns the available sample rates
	/// - note: This corresponds to `kAudioDevicePropertyAvailableNominalSampleRates`
	public func availableSampleRates(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [AudioValueRange] {
		return try getProperty(.deviceAvailableNominalSampleRates, scope: scope, element: element)
	}

	/// Returns the URL of the device's icon
	/// - note: This corresponds to `kAudioDevicePropertyIcon`
	public func icon(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> URL {
		return try getProperty(.deviceIcon, scope: scope, element: element)
	}

	/// Returns `true` if the device is hidden
	/// - note: This corresponds to `kAudioDevicePropertyIsHidden`
	public func isHidden(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceIsHidden, scope: scope, element: element) != 0
	}

	/// Returns the preferred stereo channels for the device
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelsForStereo`
	public func preferredStereoChannels(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> [UInt] {
		return try getProperty(.devicePreferredChannelsForStereo, scope: scope, element: element)
	}

	/// Returns the preferred channel layout
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelLayout`
	public func preferredChannelLayout(_ scope: PropertyScope = .global, element: PropertyElement = .master) throws -> AVAudioChannelLayout {
		return try getProperty(.devicePreferredChannelLayout, scope: scope, element: element)
	}

	/// Sets the preferred channel layout
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelLayout`
	public func setPreferredChannelLayout(_ value: AVAudioChannelLayout, scope: PropertyScope = .global, element: PropertyElement = .master) throws {
		try setProperty(.devicePreferredChannelLayout, value, scope: scope, element: element)
	}

	// MARK: - Device Properties

}
