/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioDevice {
	/// Returns `true` if this device is a private aggregate device
	/// - note: An aggregate device is private if `kAudioAggregateDeviceIsPrivateKey` in the composition dictionary is true
	/// - throws: An error if the property could not be retrieved
	public func isPrivateAggregate() throws -> Bool {
		if let aggregate = self as? AggregateDevice, try aggregate.isPrivate() {
			return true;
		}
		return false
	}

	// MARK: - Device Base Properties
	
	/// Returns the configuration application
	/// - note: This corresponds to `kAudioDevicePropertyConfigurationApplication`
	/// - throws: An error if the property could not be retrieved
	public func configurationApplication() throws -> String {
		return try getProperty(.deviceConfigurationApplication)
	}

	/// Returns the device UID
	/// - note: This corresponds to `kAudioDevicePropertyDeviceUID`
	/// - throws: An error if the property could not be retrieved
	public func deviceUID() throws -> String {
		return try getProperty(.deviceUID)
	}

	/// Returns the model UID
	/// - note: This corresponds to `kAudioDevicePropertyModelUID`
	/// - throws: An error if the property could not be retrieved
	public func modelUID() throws -> String {
		return try getProperty(.deviceModelUID)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioDevicePropertyTransportType`
	/// - throws: An error if the property could not be retrieved
	public func transportType() throws -> TransportType {
		return TransportType(rawValue: UInt32(try getProperty(.deviceTransportType) as UInt))!
	}

	/// Returns related audio devices
	/// - note: This corresponds to `kAudioDevicePropertyRelatedDevices`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func relatedDevices(_ scope: PropertyScope = .global) throws -> [AudioDevice] {
		return try getProperty(.deviceRelatedDevices, scope: scope) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the clock domain
	/// - note: This corresponds to `kAudioClockDevicePropertyClockDomain`
	/// - throws: An error if the property could not be retrieved
	public func clockDomain() throws -> UInt {
		return try getProperty(.deviceClockDomain)
	}

	/// Returns `true` if the device is alive
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsAlive`
	/// - throws: An error if the property could not be retrieved
	public func isAlive() throws -> Bool {
		return try getProperty(.deviceIsAlive) != 0
	}

	/// Returns `true` if the device is running
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsRunning`
	/// - throws: An error if the property could not be retrieved
	public func isRunning() throws -> Bool {
		return try getProperty(.deviceIsRunning) != 0
	}

	/// Starts or stops the device
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsRunning`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setIsRunning(_ value: Bool) throws {
		try setProperty(.deviceIsRunning, UInt(value ? 1 : 0))
	}

	/// Returns `true` if the device can be the default device
	/// - note: This corresponds to `kAudioDevicePropertyDeviceCanBeDefaultDevice`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func canBeDefault(_ scope: PropertyScope) throws -> Bool {
		return try getProperty(.deviceCanBeDefaultDevice, scope: scope) != 0
	}

	/// Returns `true` if the device can be the system default device
	/// - note: This corresponds to `kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func canBeSystemDefault(_ scope: PropertyScope) throws -> Bool {
		return try getProperty(.deviceCanBeDefaultSystemDevice, scope: scope) != 0
	}

	/// Returns the latency
	/// - note: This corresponds to `kAudioDevicePropertyLatency`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func latency(_ scope: PropertyScope) throws -> UInt {
		return try getProperty(.deviceLatency, scope: scope)
	}

	/// Returns the device's streams
	/// - note: This corresponds to `kAudioDevicePropertyStreams`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func streams(_ scope: PropertyScope) throws -> [AudioStream] {
		return try getProperty(.deviceStreams, scope: scope) as [AudioObject] as! [AudioStream]
	}

	/// Returns the device's audio controls
	/// - note: This corresponds to `kAudioObjectPropertyControlList`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func controls(_ scope: PropertyScope) throws -> [AudioControl] {
		return try getProperty(.controlList, scope: scope) as [AudioObject] as! [AudioControl]
	}

	/// Returns the safety offset
	/// - note: This corresponds to `kAudioDevicePropertySafetyOffset`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func safetyOffset(_ scope: PropertyScope) throws -> UInt {
		return try getProperty(.deviceSafetyOffset, scope: scope)
	}

	/// Returns the sample rate
	/// - note: This corresponds to `kAudioDevicePropertyNominalSampleRate`
	/// - throws: An error if the property could not be retrieved
	public func sampleRate() throws -> Double {
		return try getProperty(.deviceNominalSampleRate)
	}

	/// Sets the sample rate
	/// - note: This corresponds to `kAudioDevicePropertyNominalSampleRate`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setSampleRate(_ value: Double) throws {
		try setProperty(.deviceNominalSampleRate, value)
	}

	/// Returns the available sample rates
	/// - note: This corresponds to `kAudioDevicePropertyAvailableNominalSampleRates`
	/// - throws: An error if the property could not be retrieved
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try getProperty(.deviceAvailableNominalSampleRates)
	}

	/// Returns the URL of the device's icon
	/// - note: This corresponds to `kAudioDevicePropertyIcon`
	/// - throws: An error if the property could not be retrieved
	public func icon() throws -> URL {
		return try getProperty(.deviceIcon)
	}

	/// Returns `true` if the device is hidden
	/// - note: This corresponds to `kAudioDevicePropertyIsHidden`
	/// - throws: An error if the property could not be retrieved
	public func isHidden() throws -> Bool {
		return try getProperty(.deviceIsHidden) != 0
	}

	/// Returns the preferred stereo channels for the device
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelsForStereo`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func preferredStereoChannels(_ scope: PropertyScope) throws -> [UInt] {
		return try getProperty(.devicePreferredChannelsForStereo, scope: scope)
	}

	/// Returns the preferred channel layout
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelLayout`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func preferredChannelLayout(_ scope: PropertyScope) throws -> AudioChannelLayoutWrapper {
		return try getProperty(.devicePreferredChannelLayout, scope: scope)
	}

	/// Sets the preferred channel layout
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelLayout`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setPreferredChannelLayout(_ value: AudioChannelLayoutWrapper, scope: PropertyScope) throws {
		try setProperty(.devicePreferredChannelLayout, value, scope: scope)
	}

	// MARK: - Device Properties

	/// Returns any error codes loading the driver plugin
	/// - note: This corresponds to `kAudioDevicePropertyPlugIn`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func plugin(_ scope: PropertyScope = .global) throws -> OSStatus {
		return OSStatus(bitPattern: UInt32(try getProperty(.devicePlugIn, scope: scope) as UInt))
	}

	/// Returns `true` if the device is running somewhere
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsRunningSomewhere`
	/// - throws: An error if the property could not be retrieved
	public func isRunningSomewhere() throws -> Bool {
		return try getProperty(.deviceDeviceIsRunningSomewhere) != 0
	}

	/// Returns the owning pid or `-1` if the device is available to all processes
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func hogMode(_ scope: PropertyScope = .global) throws -> pid_t {
		return pid_t(bitPattern: UInt32(try getProperty(.deviceHogMode, scope: scope) as UInt))
	}

	/// Sets the owning pid
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setHogMode(_ value: pid_t, scope: PropertyScope = .global) throws {
		try setProperty(.deviceHogMode, UInt(value), scope: scope)
	}

	// Hog mode helpers

	/// Returns `true` if the device is hogged
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func isHogged(_ scope: PropertyScope = .global) throws -> Bool {
		return try hogMode(scope) != pid_t(-1)
	}

	/// Returns `true` if the device is hogged and the current process is the owner
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func isHogOwner(_ scope: PropertyScope = .global) throws -> Bool {
		return try hogMode(scope) != getpid()
	}

	/// Takes hog mode for the current pid
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func startHogging(_ scope: PropertyScope = .global) throws {
		try __startHogging(in: scope)
	}

	/// Releases hog mode
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func stopHogging(_ scope: PropertyScope = .global) throws {
		try __stopHogging(in: scope)
	}

	/// Returns the variable buffer frame size
	/// - note: This corresponds to `kAudioDevicePropertyUsesVariableBufferFrameSizes`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func usesVariableBufferFrameSizes(_ scope: PropertyScope = .global) throws -> UInt {
		return try getProperty(.deviceUsesVariableBufferFrameSizes, scope: scope)
	}

	/// Returns the IO cycle usage
	/// - note: This corresponds to `kAudioDevicePropertyIOCycleUsage`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func ioCycleUsage(_ scope: PropertyScope = .global) throws -> Float {
		return try getProperty(.deviceIOCycleUsage, scope: scope)
	}

	/// Returns the stream configuration
	/// - note: This corresponds to `kAudioDevicePropertyStreamConfiguration`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func streamConfiguration(_ scope: PropertyScope) throws -> AudioBufferListWrapper {
		return try getProperty(.deviceStreamConfiguration, scope: scope)
	}

	/// Returns IOProc stream usage
	/// - note: This corresponds to `kAudioDevicePropertyIOProcStreamUsage`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func ioProcStreamUsageInScope(_ scope: PropertyScope) throws -> Int {
//		return try getProperty(.deviceActualSampleRate, scope: scope)
		return 0
	}

	/// Returns the actual sample rate
	/// - note: This corresponds to `kAudioDevicePropertyActualSampleRate`
	/// - throws: An error if the property could not be retrieved
	public func actualSampleRate() throws -> Double {
		return try getProperty(.deviceActualSampleRate)
	}

	/// Returns the UID of the clock device
	/// - note: This corresponds to `kAudioDevicePropertyClockDevice`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func clockDevice(_ scope: PropertyScope = .global) throws -> String {
		return try getProperty(.deviceClockDevice, scope: scope)
	}

	/// Returns the workgroup to which the device's IOThread belongs
	/// - note: This corresponds to `kAudioDevicePropertyIOThreadOSWorkgroup`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func ioThreadOSWorkgroup(_ scope: PropertyScope = .global) throws -> WorkGroup {
//		return try getProperty(.deviceIOThreadOSWorkgroup, scope: scope)
		fatalError()
	}

}
