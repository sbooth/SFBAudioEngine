//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio
import os.log

/// A HAL audio device object (`kAudioDeviceClassID`)
///
/// This class has four scopes (`kAudioObjectPropertyScopeGlobal`, `kAudioObjectPropertyScopeInput`, `kAudioObjectPropertyScopeOutput`, and `kAudioObjectPropertyScopePlayThrough`), a master element (`kAudioObjectPropertyElementMaster`), and an element for each channel in each stream
public class AudioDevice: AudioObject {
	/// Returns the available audio devices (`kAudioHardwarePropertyDevices` from `kAudioObjectSystemObject`)
	public class func devices() throws -> [AudioDevice] {
		try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDevices)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the default input device (`kAudioHardwarePropertyDefaultInputDevice` from `kAudioObjectSystemObject`)
	public class func defaultInputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDefaultInputDevice))) as! AudioDevice
	}

	/// Returns the default output device (`kAudioHardwarePropertyDefaultOutputDevice` from `kAudioObjectSystemObject`)
	public class func defaultOutputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDefaultOutputDevice))) as! AudioDevice
	}

	/// Returns the default system output device (`kAudioHardwarePropertyDefaultSystemOutputDevice` from `kAudioObjectSystemObject`)
	public class func defaultSystemOutputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDefaultSystemOutputDevice))) as! AudioDevice
	}

	/// Initializes an `AudioDevice` with `uid`
	/// - parameter uid: The desired device UID
	public convenience init?(_ uid: String) {
		var qualifier = uid as CFString
		guard let deviceObjectID: AudioObjectID = try? AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToDevice), qualifier: PropertyQualifier(&qualifier)), deviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		self.init(deviceObjectID)
	}

	/// Returns `true` if the device supports input
	///
	/// - note: A device supports input if it has buffers in `kAudioObjectPropertyScopeInput` for  `kAudioDevicePropertyStreamConfiguration`
	public func supportsInput() throws -> Bool {
		try streamConfiguration(in: .input).numberBuffers > 0
	}

	/// Returns `true` if the device supports output
	///
	/// - note: A device supports output if it has buffers in `kAudioObjectPropertyScopeOutput` for `kAudioDevicePropertyStreamConfiguration`
	public func supportsOutput() throws -> Bool {
		try streamConfiguration(in: .output).numberBuffers > 0
	}
}

// MARK: - Audio Device Base Properties

extension AudioDevice {
	/// Returns the configuration application (`kAudioDevicePropertyConfigurationApplication`)
	public func configurationApplication() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyConfigurationApplication))
	}

	/// Returns the device UID (`kAudioDevicePropertyDeviceUID`)
	public func deviceUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceUID))
	}

	/// Returns the model UID (`kAudioDevicePropertyModelUID`)
	public func modelUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyModelUID))
	}

	/// Returns the transport type (`kAudioDevicePropertyTransportType`)
	public func transportType() throws -> TransportType {
		return TransportType(rawValue: try getProperty(PropertyAddress(kAudioDevicePropertyTransportType)))
	}

	/// Returns related audio devices (`kAudioDevicePropertyRelatedDevices`)
	public func relatedDevices() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioDevicePropertyRelatedDevices)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the clock domain (`kAudioClockDevicePropertyClockDomain`)
	public func clockDomain() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyClockDomain))
	}

	/// Returns `true` if the device is alive (`kAudioDevicePropertyDeviceIsAlive`)
	public func isAlive() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyModelUID)) != 0
	}

	/// Returns `true` if the device is running (`kAudioDevicePropertyDeviceIsRunning`)
	public func isRunning() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceIsRunning)) != 0
	}
	/// Starts or stops the device (`kAudioDevicePropertyDeviceIsRunning`)
	/// - parameter value: The desired property value
	public func setIsRunning(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioDevicePropertyDeviceIsRunning), to: value ? 1 : 0)
	}

	/// Returns `true` if the device can be the default device (`kAudioDevicePropertyDeviceCanBeDefaultDevice`)
	/// - parameter scope: The desired scope
	public func canBeDefault(in scope: PropertyScope) throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceCanBeDefaultDevice)) != 0
	}

	/// Returns `true` if the device can be the system default device (`kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`)
	/// - parameter scope: The desired scope
	public func canBeSystemDefault(in scope: PropertyScope) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDeviceCanBeDefaultSystemDevice), in: scope)) != 0
	}

	/// Returns the latency (`kAudioDevicePropertyLatency`)
	/// - parameter scope: The desired scope
	public func latency(in scope: PropertyScope) throws -> UInt32 {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyLatency), in: scope))
	}

	/// Returns the device's streams (`kAudioDevicePropertyStreams`)
	/// - parameter scope: The desired scope
	public func streams(in scope: PropertyScope) throws -> [AudioStream] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStreams), in: scope)).map { AudioObject.make($0) as! AudioStream }
	}

	/// Returns the device's audio controls (`kAudioObjectPropertyControlList`)
	/// - parameter scope: The desired scope
	public func controlList(in scope: PropertyScope) throws -> [AudioControl] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioObjectPropertyControlList), in: scope)).map { AudioObject.make($0) as! AudioControl }
	}

	/// Returns the safety offset (`kAudioDevicePropertySafetyOffset`)
	/// - parameter scope: The desired scope
	public func safetyOffset(in scope: PropertyScope) throws -> UInt32 {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySafetyOffset), in: scope))
	}

	/// Returns the sample rate (`kAudioDevicePropertyNominalSampleRate`)
	public func sampleRate() throws -> Double {
		return try getProperty(PropertyAddress(kAudioDevicePropertyNominalSampleRate))
	}
	/// Sets the sample rate (`kAudioDevicePropertyNominalSampleRate`)
	/// - parameter value: The desired property value
	public func setSampleRate(_ value: Double) throws {
		os_log(.info, log: audioObjectLog, "Setting device 0x%x sample rate to %.2f Hz", objectID, value)
		try setProperty(PropertyAddress(kAudioDevicePropertyNominalSampleRate), to: value)
	}

	/// Returns the available sample rates (`kAudioDevicePropertyAvailableNominalSampleRates`)
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try getProperty(PropertyAddress(kAudioDevicePropertyAvailableNominalSampleRates))
	}

	/// Returns the URL of the device's icon (`kAudioDevicePropertyIcon`)
	public func icon() throws -> URL {
		return try getProperty(PropertyAddress(kAudioDevicePropertyIcon))
	}

	/// Returns `true` if the device is hidden (`kAudioDevicePropertyIsHidden`)
	public func isHidden() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyIsHidden)) != 0
	}

	/// Returns the preferred stereo channels for the device (`kAudioDevicePropertyPreferredChannelsForStereo`)
	/// - parameter scope: The desired scope
	public func preferredStereoChannels(in scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), in: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}

	/// Sets the preferred stereo channels (`kAudioDevicePropertyPreferredChannelsForStereo`)
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	public func setPreferredStereoChannels(_ value: (UInt32, UInt32), scope: PropertyScope) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), in: scope), to: [value.0, value.1])
	}

	/// Returns the preferred channel layout (`kAudioDevicePropertyPreferredChannelLayout`)
	/// - parameter scope: The desired scope
	public func preferredChannelLayout(in scope: PropertyScope) throws -> AudioChannelLayoutWrapper {
		return try getAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelLayout), in: scope), from: objectID)
	}
	/// Sets the preferred channel layout (`kAudioDevicePropertyPreferredChannelLayout`)
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
//	public func setPreferredChannelLayout(_ value: AudioChannelLayout, in scope: PropertyScope) throws {
//		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelLayout), in: scope), to: value)
//	}
}

// MARK: - Audio Device Properties

extension AudioDevice {
	/// Returns any error codes loading the driver plugin (`kAudioDevicePropertyPlugIn`)
	public func plugIn() throws -> OSStatus {
		return try getProperty(PropertyAddress(kAudioDevicePropertyPlugIn))
	}

	/// Returns `true` if the device is running somewhere (`kAudioDevicePropertyDeviceIsRunningSomewhere`)
	public func isRunningSomewhere() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceIsRunningSomewhere)) != 0
	}

	/// Returns the owning pid or `-1` if the device is available to all processes (`kAudioDevicePropertyHogMode`)
	public func hogMode() throws -> pid_t {
		return try getProperty(PropertyAddress(kAudioDevicePropertyHogMode))
	}
	/// Sets the owning pid (`kAudioDevicePropertyHogMode`)
	public func setHogMode(_ value: pid_t) throws {
		try setProperty(PropertyAddress(kAudioDevicePropertyHogMode), to: value)
	}

	// Hog mode helpers

	/// Returns `true` if the device is hogged
	public func isHogged() throws -> Bool {
		return try hogMode() != -1
	}

	/// Returns `true` if the device is hogged and the current process is the owner
	public func isHogOwner() throws -> Bool {
		return try hogMode() != getpid()
	}

	/// Takes hog mode
	public func startHogging() throws {
		os_log(.info, log: audioObjectLog, "Taking hog mode for device 0x%x", objectID)

		let hogpid = try hogMode()
		if hogpid != -1 {
			os_log(.error, log: audioObjectLog, "Device is already hogged by pid: %d", hogpid)
		}

		try setHogMode(getpid())
	}

	/// Releases hog mode
	public func stopHogging() throws {
		os_log(.info, log: audioObjectLog, "Releasing hog mode for device 0x%x", objectID)

		let hogpid = try hogMode()
		if hogpid != getpid() {
			os_log(.error, log: audioObjectLog, "Device is hogged by pid: %d", hogpid)
		}

		try setHogMode(-1)
	}

	/// Returns the buffer size in frames (`kAudioDevicePropertyBufferFrameSize`)
	public func bufferFrameSize() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioDevicePropertyBufferFrameSize))
	}
	/// Sets the buffer size in frames (`kAudioDevicePropertyBufferFrameSize`)
	public func setBufferFrameSize(_ value: UInt32) throws {
		try setProperty(PropertyAddress(kAudioDevicePropertyBufferFrameSize), to: value)
	}

	/// Returns the minimum and maximum values for the buffer size in frames (`kAudioDevicePropertyBufferFrameSizeRange`)
	public func bufferFrameSizeRange() throws -> AudioValueRange {
		return try getProperty(PropertyAddress(kAudioDevicePropertyBufferFrameSizeRange))
	}

	/// Returns the variable buffer frame size (`kAudioDevicePropertyUsesVariableBufferFrameSizes`)
	public func usesVariableBufferFrameSizes() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioDevicePropertyHogMode))
	}

	/// Returns the IO cycle usage (`kAudioDevicePropertyIOCycleUsage`)
	public func ioCycleUsage() throws -> Float {
		return try getProperty(PropertyAddress(kAudioDevicePropertyHogMode))
	}

	/// Returns the stream configuration (`kAudioDevicePropertyStreamConfiguration`)
	public func streamConfiguration(in scope: PropertyScope) throws -> AudioBufferListWrapper {
		return try getAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStreamConfiguration), in: scope), from: objectID)
	}

	/// Returns IOProc stream usage
	/// - note: This corresponds to `kAudioDevicePropertyIOProcStreamUsage`
	/// - parameter ioProc: The desired IOProc
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
//	public func ioProcStreamUsage(_ ioProc: UnsafeMutableRawPointer, in scope: PropertyScope) throws -> AudioHardwareIOProcStreamUsageWrapper {
//		return try __ioProcStreamUsage(ioProc, in: scope)
//	}

	/// Returns the actual sample rate (`kAudioDevicePropertyActualSampleRate`)
	public func actualSampleRate() throws -> Double {
		return try getProperty(PropertyAddress(kAudioDevicePropertyActualSampleRate))
	}

	/// Returns the UID of the clock device (`kAudioDevicePropertyClockDevice`)
	public func clockDevice() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyClockDevice))
	}

	/// Returns the workgroup to which the device's IOThread belongs (`kAudioDevicePropertyIOThreadOSWorkgroup`)
	@available(macOS 11.0, *)
	public func ioThreadOSWorkgroup(in scope: PropertyScope = .global) throws -> WorkGroup {
		var value: WorkGroup = unsafeBitCast(0, to: WorkGroup.self)
		try readAudioObjectProperty(PropertyAddress(kAudioDevicePropertyIOThreadOSWorkgroup), from: objectID, into: &value)
		return value
	}
}

// MARK: - Audio Device Properties Implemented by Audio Controls

extension AudioDevice {
	/// Returns `true` if a jack is connected to `element` (`kAudioDevicePropertyJackIsConnected`)
	public func jackIsConnected(to element: PropertyElement = .master, in scope: PropertyScope = .global) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyJackIsConnected), in: scope, on: element)) != 0
	}

	/// Returns the volume scalar for `channel` (`kAudioDevicePropertyVolumeScalar`)
	public func volumeScalar(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeScalar), in: scope, on: channel))
	}
	/// Sets the volume scalar for `channel` (`kAudioDevicePropertyVolumeScalar`)
	public func setVolumeScalar(_ value: Float, channel: PropertyElement = .master, in scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeScalar), in: scope, on: channel), to: value)
	}

	/// Returns the volume decibels for `channel` (`kAudioDevicePropertyVolumeDecibels`)
	public func volumeDecibels(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeDecibels), in: scope, on: channel))
	}
	/// Sets the volume decibels for `channel` (`kAudioDevicePropertyVolumeDecibels`)
	public func setVolumeDecibels(_ value: Float, channel: PropertyElement = .master, scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeDecibels), in: scope, on: channel), to: value)
	}

	/// Returns the volume range in decibels for `channel` (`kAudioDevicePropertyVolumeRangeDecibels`)
	public func volumeRangeDecibels(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> AudioValueRange {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeRangeDecibels), in: scope, on: channel))
	}

	/// Returns the stereo pan (`kAudioDevicePropertyStereoPan`)
	public func stereoPan(in scope: PropertyScope) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPan), in: scope))
	}
	/// Sets the stereo pan (`kAudioDevicePropertyStereoPan`)
	public func setStereoPan(_ value: Float, in scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPan), in: scope), to: value)
	}

	/// Returns the channels used for stereo panning (`kAudioDevicePropertyStereoPanChannels`)
	public func stereoPanChannels(in scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPanChannels), in: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}
	/// Sets the channels used for stereo panning (`kAudioDevicePropertyStereoPanChannels`)
	public func setStereoPanChannels(_ value: (UInt32, UInt32), scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPanChannels), in: scope), to: [value.0, value.1])
	}

	/// Returns `true` if `element` is muted (`kAudioDevicePropertyMute`)
	public func mute(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyMute), in: scope, on: element)) != 0
	}
	/// Sets whether `element` is muted (`kAudioDevicePropertyMute`)
	public func setMute(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws{
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyMute), in: scope, on: element), to: value ? 1 : 0)
	}

	/// Returns `true` if only `element` is audible (`kAudioDevicePropertySolo`)
	public func solo(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySolo), in: scope, on: element)) != 0
	}
	/// Sets whether `element` is audible (`kAudioDevicePropertySolo`)
	public func setSolo(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws{
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySolo), in: scope, on: element), to: value ? 1 : 0)
	}

	/// Returns `true` if phantom power is enabled for `element` (`kAudioDevicePropertyPhantomPower`)
	public func phantomPower(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhantomPower), in: scope, on: element)) != 0
	}
	/// Sets whether phantom power is enabled for `element` (`kAudioDevicePropertyPhantomPower`)
	public func setPhantomPower(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws{
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhantomPower), in: scope, on: element), to: value ? 1 : 0)
	}

	/// Returns `true` if the phase is inverted for `element` (`kAudioDevicePropertyPhaseInvert`)
	public func phaseInvert(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhaseInvert), in: scope, on: element)) != 0
	}
	/// Sets whether the phase is inverted for `element` (`kAudioDevicePropertyPhaseInvert`)
	public func setPhaseInvert(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws{
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhaseInvert), in: scope, on: element), to: value ? 1 : 0)
	}

	/// Returns `true` if the signal exceeded the sample range (`kAudioDevicePropertyClipLight`)
	public func clipLight(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClipLight), in: scope, on: element)) != 0
	}
	/// Sets whether the signal exceeded the sample range (`kAudioDevicePropertyClipLight`)
	public func setClipLight(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws{
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClipLight), in: scope, on: element), to: value ? 1 : 0)
	}

	/// Returns `true` if talkback is enabled (`kAudioDevicePropertyTalkback`)
	public func talkback(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyTalkback), in: scope, on: element)) != 0
	}
	/// Sets whether talkback is enabled (`kAudioDevicePropertyTalkback`)
	public func setTalkback(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws{
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyTalkback), in: scope, on: element), to: value ? 1 : 0)
	}

	/// Returns `true` if listenback is enabled (`kAudioDevicePropertyListenback`)
	public func listenback(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyListenback), in: scope, on: element)) != 0
	}
	/// Sets whether listenback is enabled (`kAudioDevicePropertyListenback`)
	public func setListenback(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws{
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyListenback), in: scope, on: element), to: value ? 1 : 0)
	}

	/// Returns the IDs of all the currently selected data sources (`kAudioDevicePropertyDataSource`)
	public func dataSource(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSource), in: scope))
	}

	/// Sets the currently selected data sources (`kAudioDevicePropertyDataSource` )
	public func setDataSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSource), in: scope), to: value)
	}

	/// Returns the IDs of all the currently available data sources (`kAudioDevicePropertyDataSources`)
	public func dataSources(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSources), in: scope))
	}

	/// Returns the name of `dataSourceID` (`kAudioDevicePropertyDataSourceNameForIDCFString`)
	public func dataSourceName(_ dataSourceID: UInt32, scope: PropertyScope) throws -> String {
		var inputData = dataSourceID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSourceNameForIDCFString), in: scope), from: objectID, into: &translation)
			}
		}
		return outputData as String
	}

	/// Returns the kind of `dataSourceID` (`kAudioDevicePropertyDataSourceKindForID`)
	public func dataSourceKind(_ dataSourceID: UInt32, scope: PropertyScope) throws -> UInt32 {
		var inputData = dataSourceID
		var outputData: UInt32 = 0
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<UInt32>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSourceKindForID), in: scope), from: objectID, into: &translation)
			}
		}
		return outputData
	}

	// Data source helpers

	/// Returns the available data sources (`kAudioDevicePropertyDataSources`)
	public func availableDataSources(in scope: PropertyScope) throws -> [DataSource] {
		return try dataSources(in: scope).map { DataSource(audioDevice: self, scope: scope, dataSourceID: $0) }
	}

	/// Returns the active  data sources (`kAudioDevicePropertyDataSource`)
	public func activeDataSources(in scope: PropertyScope) throws -> [DataSource] {
		return try dataSource(in: scope).map { DataSource(audioDevice: self, scope: scope, dataSourceID: $0) }
	}

	/// Returns the IDs of all the currently selected clock sources (`kAudioDevicePropertyClockSource`)
	public func clockSource(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSource), in: scope))
	}

	/// Sets the currently selected clock sources (`kAudioDevicePropertyClockSource`)
	public func setClockSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSource), in: scope), to: value)
	}

	/// Returns the IDs of all the currently available clock sources (`kAudioDevicePropertyClockSources`)
	public func clockSources(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSources), in: scope))
	}

	/// Returns the name of `clockSourceID` (`kAudioDevicePropertyClockSourceNameForIDCFString`)
	public func clockSourceName(_ clockSourceID: UInt32, scope: PropertyScope) throws -> String {
		var inputData = clockSourceID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSourceNameForIDCFString), in: scope), from: objectID, into: &translation)
			}
		}
		return outputData as String
	}

	/// Returns the kind of `clockSourceID` (`kAudioDevicePropertyClockSourceKindForID`)
	/// - parameter clockSourceID: The desired clock source
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func clockSourceKind(_ clockSourceID: UInt32, scope: PropertyScope) throws -> UInt32 {
		var inputData = clockSourceID
		var outputData: UInt32 = 0
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<UInt32>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSourceKindForID), in: scope), from: objectID, into: &translation)
			}
		}
		return outputData
	}

	// Clock source helpers

	/// Returns the available clock sources (`kAudioDevicePropertyClockSources`)
	public func availableClockSources(in scope: PropertyScope) throws -> [ClockSource] {
		return try clockSources(in: scope).map { ClockSource(audioDevice: self, scope: scope, clockSourceID: $0) }
	}

	/// Returns the active  clock sources (`kAudioDevicePropertyClockSource`)
	public func activeClockSources(in scope: PropertyScope) throws -> [ClockSource] {
		return try clockSource(in: scope).map { ClockSource(audioDevice: self, scope: scope, clockSourceID: $0) }
	}

	/// Returns `true` if play through is enabled (`kAudioDevicePropertyPlayThru`)
	public func playThru(on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPlayThru), in: .playThrough, on: element)) != 0
	}

//	/// Returns `true` if only the specified play through element is audible
//	/// - note: This corresponds to `kAudioDevicePropertyPlayThruSolo`
//	/// - parameter scope: The desired scope
//	/// - parameter element: The desired element
//	/// - throws: An error if the property could not be retrieved
//	public func playThruSolo(on element: PropertyElement = .master) throws -> Bool {
//		return try getProperty(.devicePlayThruSolo, scope: .playThrough, element: element) != 0
//	}
}

extension AudioDevice {
	/// A thin wrapper around a HAL audio device transport type
	public struct TransportType: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
		/// Unknown
		public static let unknown 		= TransportType(rawValue: kAudioDeviceTransportTypeUnknown)
		/// Built-in
		public static let builtIn 		= TransportType(rawValue: kAudioDeviceTransportTypeBuiltIn)
		/// Aggregate device
		public static let aggregate 	= TransportType(rawValue: kAudioDeviceTransportTypeAggregate)
		/// Virtual device
		public static let virtual 		= TransportType(rawValue: kAudioDeviceTransportTypeVirtual)
		/// PCI
		public static let pci 			= TransportType(rawValue: kAudioDeviceTransportTypePCI)
		/// USB
		public static let usb 			= TransportType(rawValue: kAudioDeviceTransportTypeUSB)
		/// FireWire
		public static let fireWire 		= TransportType(rawValue: kAudioDeviceTransportTypeFireWire)
		/// Bluetooth
		public static let bluetooth 	= TransportType(rawValue: kAudioDeviceTransportTypeBluetooth)
		/// Bluetooth Low Energy
		public static let bluetoothLE 	= TransportType(rawValue: kAudioDeviceTransportTypeBluetoothLE)
		/// HDMI
		public static let hdmi 			= TransportType(rawValue: kAudioDeviceTransportTypeHDMI)
		/// DisplayPort
		public static let displayPort 	= TransportType(rawValue: kAudioDeviceTransportTypeDisplayPort)
		/// AirPlay
		public static let airPlay 		= TransportType(rawValue: kAudioDeviceTransportTypeAirPlay)
		/// AVB
		public static let avb 			= TransportType(rawValue: kAudioDeviceTransportTypeAVB)
		/// Thunderbolt
		public static let thunderbolt 	= TransportType(rawValue: kAudioDeviceTransportTypeThunderbolt)

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

extension AudioDevice.TransportType: CustomDebugStringConvertible {
	public var debugDescription: String {
		switch self.rawValue {
		case kAudioDeviceTransportTypeUnknown:		return "Unknown"
		case kAudioDeviceTransportTypeBuiltIn:		return "Built-in"
		case kAudioDeviceTransportTypeAggregate: 	return "Aggregate"
		case kAudioDeviceTransportTypeVirtual:		return "Virtual"
		case kAudioDeviceTransportTypePCI:			return "PCI"
		case kAudioDeviceTransportTypeUSB:			return "USB"
		case kAudioDeviceTransportTypeFireWire:		return "FireWire"
		case kAudioDeviceTransportTypeBluetooth:	return "Bluetooth"
		case kAudioDeviceTransportTypeBluetoothLE: 	return "Bluetooth Low Energy"
		case kAudioDeviceTransportTypeHDMI:			return "HDMI"
		case kAudioDeviceTransportTypeDisplayPort:	return "DisplayPort"
		case kAudioDeviceTransportTypeAirPlay:		return "AirPlay"
		case kAudioDeviceTransportTypeAVB:			return "AVB"
		case kAudioDeviceTransportTypeThunderbolt: 	return "Thunderbolt"
		default:									return "\(self.rawValue)"
		}
	}
}

extension AudioDevice {
	/// A data source for an audio device
	public struct DataSource {
		/// Returns the owning audio device
		public let audioDevice: AudioDevice
		/// Returns the data source scope
		public let scope: PropertyScope
		/// Returns the data source ID
		public let dataSourceID: UInt32

		/// Returns the data source name
		public func name() throws -> String {
			return try audioDevice.dataSourceName(dataSourceID, scope: scope)
		}

		/// Returns the data source kind
		public func kind() throws -> UInt32 {
			return try audioDevice.dataSourceKind(dataSourceID, scope: scope)
		}
	}
}

extension AudioDevice.DataSource: CustomDebugStringConvertible {
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)) (\(scope), '\(dataSourceID.fourCC)') \"\(name)\" on AudioDevice 0x\(String(audioDevice.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)) (\(scope), '\(dataSourceID.fourCC)') on AudioDevice 0x\(String(audioDevice.objectID, radix: 16, uppercase: false)))>"
		}
	}
}

extension AudioDevice {
	/// A clock source for an audio device
	public struct ClockSource {
		/// Returns the owning audio device
		public let audioDevice: AudioDevice
		/// Returns the clock source scope
		public let scope: PropertyScope
		/// Returns the clock source ID
		public let clockSourceID: UInt32

		/// Returns the clock source name
		public func name() throws -> String {
			return try audioDevice.clockSourceName(clockSourceID, scope: scope)
		}

		/// Returns the clock source kind
		public func kind() throws -> UInt32 {
			return try audioDevice.clockSourceKind(clockSourceID, scope: scope)
		}
	}
}

extension AudioDevice.ClockSource: CustomDebugStringConvertible {
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)) (\(scope), '\(clockSourceID.fourCC)') \"\(name)\" on AudioDevice 0x\(String(audioDevice.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)) (\(scope), '\(clockSourceID.fourCC)') on AudioDevice 0x\(String(audioDevice.objectID, radix: 16, uppercase: false)))>"
		}
	}
}
