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
		return TransportType(rawValue: try getProperty(.deviceTransportType))!
	}

	/// Returns related audio devices
	/// - note: This corresponds to `kAudioDevicePropertyRelatedDevices`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func relatedDevices() throws -> [AudioDevice] {
		return try getProperty(.deviceRelatedDevices) as [AudioObject] as! [AudioDevice]
	}

	/// Returns the clock domain
	/// - note: This corresponds to `kAudioClockDevicePropertyClockDomain`
	/// - throws: An error if the property could not be retrieved
	public func clockDomain() throws -> UInt32 {
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
		try setProperty(.deviceIsRunning, UInt32(value ? 1 : 0))
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
	public func latency(_ scope: PropertyScope) throws -> UInt32 {
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
	public func safetyOffset(_ scope: PropertyScope) throws -> UInt32 {
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
	public func preferredStereoChannels(_ scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(.devicePreferredChannelsForStereo, scope: scope)
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}

	/// Sets the preferred stereo channels
	/// - note: This corresponds to `kAudioDevicePropertyPreferredChannelsForStereo`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setPreferredStereoChannels(_ value: (UInt32, UInt32), scope: PropertyScope) throws {
		try setProperty(.devicePreferredChannelsForStereo, [value.0, value.1], scope: scope)
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
	/// - throws: An error if the property could not be retrieved
	public func plugIn() throws -> OSStatus {
		return OSStatus(bitPattern: try getProperty(.devicePlugIn))
	}

	/// Returns `true` if the device is running somewhere
	/// - note: This corresponds to `kAudioDevicePropertyDeviceIsRunningSomewhere`
	/// - throws: An error if the property could not be retrieved
	public func isRunningSomewhere() throws -> Bool {
		return try getProperty(.deviceDeviceIsRunningSomewhere) != 0
	}

	/// Returns the owning pid or `-1` if the device is available to all processes
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - throws: An error if the property could not be retrieved
	public func hogMode() throws -> pid_t {
		return pid_t(bitPattern: try getProperty(.deviceHogMode))
	}

	// Hog mode helpers

	/// Returns `true` if the device is hogged
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - throws: An error if the property could not be set
	public func isHogged() throws -> Bool {
		return try hogMode() != pid_t(-1)
	}

	/// Returns `true` if the device is hogged and the current process is the owner
	/// - note: This corresponds to `kAudioDevicePropertyHogMode`
	/// - throws: An error if the property could not be set
	public func isHogOwner() throws -> Bool {
		return try hogMode() != getpid()
	}

	/// Returns the buffer size in frames
	/// - note: This corresponds to `kAudioDevicePropertyBufferFrameSize`
	/// - throws: An error if the property could not be retrieved
	public func bufferFrameSize() throws -> UInt32 {
		return try getProperty(.deviceBufferFrameSize)
	}

	/// Sets the buffer size in frames
	/// - note: This corresponds to `kAudioDevicePropertyBufferFrameSize`
	/// - parameter value: The desired property value
	/// - throws: An error if the property could not be set
	public func setBufferFrameSize(_ value: UInt32) throws {
		try setProperty(.deviceBufferFrameSize, value)
	}

	/// Returns the minimum and maximum values for the buffer size in frames
	/// - note: This corresponds to `kAudioDevicePropertyBufferFrameSizeRange`
	/// - throws: An error if the property could not be retrieved
	public func bufferFrameSizeRange() throws -> AudioValueRange {
		return try getProperty(.deviceBufferFrameSizeRange)
	}

	/// Returns the variable buffer frame size
	/// - note: This corresponds to `kAudioDevicePropertyUsesVariableBufferFrameSizes`
	/// - throws: An error if the property could not be retrieved
	public func usesVariableBufferFrameSizes() throws -> UInt32 {
		return try getProperty(.deviceUsesVariableBufferFrameSizes)
	}

	/// Returns the IO cycle usage
	/// - note: This corresponds to `kAudioDevicePropertyIOCycleUsage`
	/// - throws: An error if the property could not be retrieved
	public func ioCycleUsage() throws -> Float {
		return try getProperty(.deviceIOCycleUsage)
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
	public func ioProcStreamUsage(_ scope: PropertyScope) throws -> AudioHardwareIOProcStreamUsageWrapper {
		return try getProperty(.deviceIOProcStreamUsage, scope: scope)
	}

	/// Returns the actual sample rate
	/// - note: This corresponds to `kAudioDevicePropertyActualSampleRate`
	/// - throws: An error if the property could not be retrieved
	public func actualSampleRate() throws -> Double {
		return try getProperty(.deviceActualSampleRate)
	}

	/// Returns the UID of the clock device
	/// - note: This corresponds to `kAudioDevicePropertyClockDevice`
	/// - throws: An error if the property could not be retrieved
	public func clockDevice() throws -> String {
		return try getProperty(.deviceClockDevice)
	}

	/// Returns the workgroup to which the device's IOThread belongs
	/// - note: This corresponds to `kAudioDevicePropertyIOThreadOSWorkgroup`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	@available(macOS 11.0, *)
	public func ioThreadOSWorkgroup(_ scope: PropertyScope = .global) throws -> WorkGroup {
		return try getProperty(.deviceIOThreadOSWorkgroup, scope: scope)
	}

	// MARK: - Device Properties Implemented by Audio Controls

	/// Returns `true` if a jack is connected to the specified element
	/// - note: This corresponds to `kAudioDevicePropertyJackIsConnected`
	/// - parameter element: The desired element
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func jackIsConnected(_ element: PropertyElement = .master, scope: PropertyScope = .global) throws -> Bool {
		return try getProperty(.deviceJackIsConnected, scope: scope, element: element) != 0
	}

	/// Returns the volume scalar for the specified channel
	/// - note: This corresponds to `kAudioDevicePropertyVolumeScalar`
	/// - parameter channel: The desired channel
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func volumeScalar(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> Float {
		return try getProperty(.deviceVolumeScalar, scope: scope, element: channel)
	}

	/// Sets the volume scalar for the specified channel
	/// - note: This corresponds to `kAudioDevicePropertyVolumeScalar`
	/// - parameter value: The desired property value
	/// - parameter channel: The desired channel
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setVolumeScalar(_ value: Float, channel: PropertyElement = .master, scope: PropertyScope = .global) throws {
		return try setProperty(.deviceVolumeScalar, value, scope: scope, element: channel)
	}

	/// Returns the volume decibels for the specified channel
	/// - note: This corresponds to `kAudioDevicePropertyVolumeDecibels`
	/// - parameter channel: The desired channel
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func volumeDecibels(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> Float {
		return try getProperty(.deviceVolumeDecibels, scope: scope, element: channel)
	}

	/// Sets the volume scalar for the specified channel
	/// - note: This corresponds to `kAudioDevicePropertyVolumeDecibels`
	/// - parameter value: The desired property value
	/// - parameter channel: The desired channel
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setVolumeDecibels(_ value: Float, channel: PropertyElement = .master, scope: PropertyScope = .global) throws {
		return try setProperty(.deviceVolumeDecibels, value, scope: scope, element: channel)
	}

	/// Returns the volume range in decibels for the specified channel
	/// - note: This corresponds to `kAudioDevicePropertyVolumeRangeDecibels`
	/// - parameter channel: The desired channel
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func volumeRangeDecibels(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> AudioValueRange {
		return try getProperty(.deviceVolumeRangeDecibels, scope: scope, element: channel)
	}

	/// Returns the stereo pan
	/// - note: This corresponds to `kAudioDevicePropertyStereoPan`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func stereoPan(_ scope: PropertyScope) throws -> Float {
		return try getProperty(.deviceStereoPan, scope: scope)
	}

	/// Returns the channels used for stereo panning
	/// - note: This corresponds to `kAudioDevicePropertyStereoPanChannels`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func stereoPanChannels(_ scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(.deviceStereoPanChannels, scope: scope)
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}

	/// Sets the channels used for stereo panning
	/// - note: This corresponds to `kAudioDevicePropertyStereoPanChannels`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setStereoPanChannels(_ value: (UInt32, UInt32), scope: PropertyScope) throws {
		try setProperty(.deviceStereoPanChannels, [value.0, value.1], scope: scope)
	}

	/// Returns `true` if the element is muted
	/// - note: This corresponds to `kAudioDevicePropertyMute`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func mute(_ scope: PropertyScope, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceMute, scope: scope, element: element) != 0
	}

	/// Returns `true` if only the specified element is audible
	/// - note: This corresponds to `kAudioDevicePropertySolo`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func solo(_ scope: PropertyScope, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceSolo, scope: scope, element: element) != 0
	}

	/// Returns `true` if phantom power is enabled for the specified element
	/// - note: This corresponds to `kAudioDevicePropertyPhantomPower`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func phantomPower(_ scope: PropertyScope, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.devicePhantomPower, scope: scope, element: element) != 0
	}

	/// Returns `true` if the phase is inverted for the specified element
	/// - note: This corresponds to `kAudioDevicePropertyPhaseInvert`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func phaseInvert(_ scope: PropertyScope, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.devicePhaseInvert, scope: scope, element: element) != 0
	}

	/// Returns `true` if the signal exceeded the sample range
	/// - note: This corresponds to `kAudioDevicePropertyClipLight`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func clipLight(_ scope: PropertyScope, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceClipLight, scope: scope, element: element) != 0
	}

	/// Returns `true` if talkback is enabled
	/// - note: This corresponds to `kAudioDevicePropertyTalkback`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func talkback(_ scope: PropertyScope, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceTalkback, scope: scope, element: element) != 0
	}

	/// Returns `true` if listenback is enabled
	/// - note: This corresponds to `kAudioDevicePropertyListenback`
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if the property could not be retrieved
	public func listenback(_ scope: PropertyScope, element: PropertyElement = .master) throws -> Bool {
		return try getProperty(.deviceListenback, scope: scope, element: element) != 0
	}

	/// Returns the IDs of all the currently selected data sources
	/// - note: This corresponds to `kAudioDevicePropertyDataSource`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func dataSource(_ scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(.deviceDataSource, scope: scope)
	}

	/// Sets the currently selected data sources
	/// - note: This corresponds to `kAudioDevicePropertyDataSource`
	/// - parameter value: The desired property value
	/// - parameter channel: The desired channel
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setDataSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(.deviceDataSource, value, scope: scope)
	}

	/// Returns the IDs of all the currently available data sources
	/// - note: This corresponds to `kAudioDevicePropertyDataSources`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func dataSources(_ scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(.deviceDataSources, scope: scope)
	}

	/// Returns the name of `dataSourceID`
	/// - note: This corresponds to `kAudioDevicePropertyDataSourceNameForIDCFString`
	/// - parameter dataSourceID: The desired data source
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func dataSourceName(_ dataSourceID: UInt32, scope: PropertyScope) throws -> String {
		return try translateValue(dataSourceID, using: .deviceDataSourceNameForIDCFString, scope: scope)
	}

	/// Returns the kind of `dataSourceID`
	/// - note: This corresponds to `kAudioDevicePropertyDataSourceKindForID`
	/// - parameter dataSourceID: The desired data source
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func dataSourceKind(_ dataSourceID: UInt32, scope: PropertyScope) throws -> UInt32 {
		return try translateValue(dataSourceID, using: .deviceDataSourceKindForID, scope: scope)
	}

	// Data source helpers

	/// Returns the available data sources
	/// - note: This corresponds to `kAudioDevicePropertyDataSources`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func availableDataSources(_ scope: PropertyScope) throws -> [DataSource] {
		let dataSourceIDs = try dataSources(scope)
		return dataSourceIDs.map { DataSource(audioDevice: self, scope: scope, dataSourceID: $0) }
	}

	/// Returns the active  data sources
	/// - note: This corresponds to `kAudioDevicePropertyDataSource`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func activeDataSources(_ scope: PropertyScope) throws -> [DataSource] {
		let dataSourceIDs = try dataSource(scope)
		return dataSourceIDs.map { DataSource(audioDevice: self, scope: scope, dataSourceID: $0) }
	}

	/// Returns the IDs of all the currently selected clock sources
	/// - note: This corresponds to `kAudioDevicePropertyClockSource`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func clockSource(_ scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(.deviceClockSource, scope: scope)
	}

	/// Sets the currently selected clock sources
	/// - note: This corresponds to `kAudioDevicePropertyClockSource`
	/// - parameter value: The desired property value
	/// - parameter channel: The desired channel
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be set
	public func setClockSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(.deviceClockSource, value, scope: scope)
	}

	/// Returns the IDs of all the currently available clock sources
	/// - note: This corresponds to `kAudioDevicePropertyClockSources`
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func clockSources(_ scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(.deviceClockSources, scope: scope)
	}

	/// Returns the name of `clockSourceID`
	/// - note: This corresponds to `kAudioDevicePropertyClockSourceNameForIDCFString`
	/// - parameter clockSourceID: The desired clock source
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func clockSourceName(_ clockSourceID: UInt32, scope: PropertyScope) throws -> String {
		return try translateValue(clockSourceID, using: .deviceClockSourceNameForIDCFString, scope: scope)
	}

	/// Returns the kind of `clockSourceID`
	/// - note: This corresponds to `kAudioDevicePropertyClockSourceKindForID`
	/// - parameter clockSourceID: The desired clock source
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func clockSourceKind(_ clockSourceID: UInt32, scope: PropertyScope) throws -> UInt32 {
		return try translateValue(clockSourceID, using: .deviceClockSourceKindForID, scope: scope)
	}
}
