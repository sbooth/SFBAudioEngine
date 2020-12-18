/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioDevice {
	/// Returns the configuration application
	/// - note: This corresponds to `kAudioDevicePropertyConfigurationApplication`
	public func configurationApplication() throws -> String {
		return try stringForProperty(.deviceConfigurationApplication)
	}

	/// Returns the device UID
	/// - note: This corresponds to `kAudioDevicePropertyDeviceUID`
	public func deviceUID() throws -> String {
		return try stringForProperty(.deviceUID)
	}

	/// Returns the model UID
	/// - note: This corresponds to `kAudioDevicePropertyModelUID`
	public func modelUID() throws -> String {
		return try stringForProperty(.deviceModelUID)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioDevicePropertyTransportType`
	public func transportType() throws -> TransportType {
		return TransportType(rawValue: UInt32(try uintForProperty(.deviceTransportType)))!
	}

	/// Returns related audio devices
	/// - note: This corresponds to `kAudioDevicePropertyRelatedDevices`
	public func relatedDevices() throws -> [AudioDevice] {
		return try audioObjectsForProperty(.deviceRelatedDevices) as! [AudioDevice]
	}

	/// Returns the clock domain
	/// - note: This corresponds to `kAudioClockDevicePropertyClockDomain`
	public func clockDomain() throws -> UInt {
		return try uintForProperty(.deviceClockDomain)
	}

	/// Returns `true` if the device is alive
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsAlive`
	public func isAlive() throws -> Bool {
		return try uintForProperty(.deviceIsAlive) != 0
	}

	/// Returns `true` if the device is running
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsRunning`
	public func isRunning() throws -> Bool {
		return try uintForProperty(.deviceIsRunning) != 0
	}

	/// Returns `true` if the device can be the default device
	/// - note: This corresponds to `kAudioDevicePropertyDeviceCanBeDefaultDevice`
	public func canBeDefault(_ scope: PropertyScope) throws -> Bool {
		return try uintForProperty(.deviceCanBeDefaultDevice, scope: scope) != 0
	}

	/// Returns `true` if the device can be the system default device
	/// - note: This corresponds to `kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`
	public func canBeSystemDefault(_ scope: PropertyScope) throws -> Bool {
		return try uintForProperty(.deviceCanBeDefaultSystemDevice, scope: scope) != 0
	}

	/// Returns the latency
	/// - note: This corresponds to `kAudioDevicePropertyLatency`
	public func latency(_ scope: PropertyScope) throws -> UInt {
		return try uintForProperty(.deviceLatency, scope: scope)
	}

	/// Returns the device's streams
	/// - note: This corresponds to `kAudioDevicePropertyStreams`
	public func streams(_ scope: PropertyScope) throws -> [AudioStream] {
		return try audioObjectsForProperty(.deviceStreams, scope: scope) as! [AudioStream]
	}

	/// Returns the device's audio controls
	/// - note: This corresponds to `kAudioObjectPropertyControlList`
	public func controls() throws -> [AudioControl] {
		return try audioObjectsForProperty(.controlList) as! [AudioControl]
	}

	/// Returns the safety offset
	/// - note: This corresponds to `kAudioDevicePropertySafetyOffset`
	public func safetyOffset(_ scope: PropertyScope) throws -> UInt {
		return try uintForProperty(.deviceSafetyOffset, scope: scope)
	}

	/// Returns the sample rate
	/// - note: This corresponds to `kAudioDevicePropertyNominalSampleRate`
	public func sampleRate() throws -> Double {
		return try doubleForProperty(.deviceNominalSampleRate)
	}

//	/// Sets the sample rate
//	/// - note: This corresponds to `kAudioDevicePropertyNominalSampleRate`
//	public func setSampleRate(_ sampleRate: Double) throws {
//		try setDouble(sampleRate, property: .deviceNominalSampleRate)
//	}


	/// Returns the available sample rates
	/// - note: This corresponds to `kAudioDevicePropertyAvailableNominalSampleRates`
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try audioValueRangesForProperty(.deviceAvailableNominalSampleRates)
	}

	/// Returns the URL of the device's icon
	/// - note: This corresponds to `kAudioDevicePropertyIcon`
	public func icon() throws -> URL {
		return try urlForProperty(.deviceIcon)
	}

	/// Returns `true` if the device is hidden
	/// - note: This corresponds to `kAudioDevicePropertyIsHidden`
	public func isHidden() throws -> Bool {
		return try uintForProperty(.deviceIsHidden) != 0
	}

	/// Returns the preferred stereo channels for the device
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelsForStereo`
	public func preferredStereoChannels(_ scope: PropertyScope) throws -> [UInt] {
		return try uintsForProperty(.devicePreferredChannelsForStereo, scope: scope)
	}

	// PREFERRED CHANNEL LAYOUT

	public func foo()
	{

		let props: [AudioObjectPropertySelector] = [
			kAudioDevicePropertyConfigurationApplication,
			kAudioDevicePropertyDeviceUID,
			kAudioDevicePropertyModelUID,
			kAudioDevicePropertyTransportType,
			kAudioDevicePropertyRelatedDevices,
			kAudioDevicePropertyClockDomain,
			kAudioDevicePropertyDeviceIsAlive,
			kAudioDevicePropertyDeviceIsRunning,
			kAudioDevicePropertyDeviceCanBeDefaultDevice,
			kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
			kAudioDevicePropertyLatency,
			kAudioDevicePropertyStreams,
			kAudioObjectPropertyControlList,
			kAudioDevicePropertySafetyOffset,
			kAudioDevicePropertyNominalSampleRate,
			kAudioDevicePropertyAvailableNominalSampleRates,
			kAudioDevicePropertyIcon,
			kAudioDevicePropertyIsHidden,
			kAudioDevicePropertyPreferredChannelsForStereo,
			kAudioDevicePropertyPreferredChannelLayout
		]

		let scopes: [PropertyScope] = [.global, .input, .output, .playThrough]

		for prop in props {
			for scope in scopes {
				let p = PropertySelector(rawValue: prop)!
				let has = hasProperty(p, scope: scope)
				print("hasProperty( \(p), \(scope) ) = \(has)")
			}
			print("\n")
		}
	}
}
