//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio
import os.log

/// A HAL audio device object
///
/// This class has four scopes (`kAudioObjectPropertyScopeGlobal`, `kAudioObjectPropertyScopeInput`, `kAudioObjectPropertyScopeOutput`, and `kAudioObjectPropertyScopePlayThrough`), a master element (`kAudioObjectPropertyElementMaster`), and an element for each channel in each stream
/// - remark: This class correponds to objects with base class `kAudioDeviceClassID`
public class AudioDevice: AudioObject {
	/// Returns the available audio devices
	/// - remark: This corresponds to the property`kAudioHardwarePropertyDevices` on `kAudioObjectSystemObject`
	public class func devices() throws -> [AudioDevice] {
		try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDevices)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the default input device
	/// - remark: This corresponds to the property`kAudioHardwarePropertyDefaultInputDevice` on `kAudioObjectSystemObject`
	public class func defaultInputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDefaultInputDevice))) as! AudioDevice
	}

	/// Returns the default output device
	/// - remark: This corresponds to the property`kAudioHardwarePropertyDefaultOutputDevice` on `kAudioObjectSystemObject`
	public class func defaultOutputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDefaultOutputDevice))) as! AudioDevice
	}

	/// Returns the default system output device
	/// - remark: This corresponds to the property`kAudioHardwarePropertyDefaultSystemOutputDevice` on `kAudioObjectSystemObject`
	public class func defaultSystemOutputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyDefaultSystemOutputDevice))) as! AudioDevice
	}

	/// Returns an initialized `AudioDevice` with `uid` or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateUIDToDevice` on `kAudioObjectSystemObject`
	/// - parameter uid: The desired device UID
	public class func makeDevice(_ uid: String) throws -> AudioDevice? {
		var qualifier = uid as CFString
		let objectID: AudioObjectID = try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToDevice), qualifier: PropertyQualifier(&qualifier))
		guard objectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(objectID) as! AudioDevice)
	}

	/// Initializes an `AudioDevice` with `uid`
	/// - remark: This corresponds to the property`kAudioHardwarePropertyTranslateUIDToDevice` on `kAudioObjectSystemObject`
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
	/// - note: A device supports input if it has buffers in `kAudioObjectPropertyScopeInput` for the property `kAudioDevicePropertyStreamConfiguration`
	public func supportsInput() throws -> Bool {
		try streamConfiguration(in: .input).numberBuffers > 0
	}

	/// Returns `true` if the device supports output
	///
	/// - note: A device supports output if it has buffers in `kAudioObjectPropertyScopeOutput` for the property `kAudioDevicePropertyStreamConfiguration`
	public func supportsOutput() throws -> Bool {
		try streamConfiguration(in: .output).numberBuffers > 0
	}
}

// MARK: - Audio Device Base Properties

extension AudioDevice {
	/// Returns the configuration application
	/// - remark: This corresponds to the property `kAudioDevicePropertyConfigurationApplication`
	public func configurationApplication() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyConfigurationApplication))
	}

	/// Returns the device UID
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceUID`
	public func deviceUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceUID))
	}

	/// Returns the model UID
	/// - remark: This corresponds to the property `kAudioDevicePropertyModelUID`
	public func modelUID() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyModelUID))
	}

	/// Returns the transport type
	/// - remark: This corresponds to the property `kAudioDevicePropertyTransportType`
	public func transportType() throws -> TransportType {
		return TransportType(rawValue: try getProperty(PropertyAddress(kAudioDevicePropertyTransportType)))
	}

	/// Returns related audio devices
	/// - remark: This corresponds to the property `kAudioDevicePropertyRelatedDevices`
	public func relatedDevices() throws -> [AudioDevice] {
		return try getProperty(PropertyAddress(kAudioDevicePropertyRelatedDevices)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the clock domain
	/// - remark: This corresponds to the property `kAudioClockDevicePropertyClockDomain`
	public func clockDomain() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioClockDevicePropertyClockDomain))
	}

	/// Returns `true` if the device is alive
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceIsAlive`
	public func isAlive() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyModelUID)) as UInt32 != 0
	}

	/// Returns `true` if the device is running
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceIsRunning`
	public func isRunning() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceIsRunning)) as UInt32 != 0
	}
	/// Starts or stops the device
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceIsRunning`
	/// - parameter value: The desired property value
	public func setIsRunning(_ value: Bool) throws {
		try setProperty(PropertyAddress(kAudioDevicePropertyDeviceIsRunning), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the device can be the default device
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceCanBeDefaultDevice`
	/// - parameter scope: The desired scope
	public func canBeDefault(in scope: PropertyScope) throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceCanBeDefaultDevice)) as UInt32 != 0
	}

	/// Returns `true` if the device can be the system default device
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`
	/// - parameter scope: The desired scope
	public func canBeSystemDefault(in scope: PropertyScope) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDeviceCanBeDefaultSystemDevice), scope: scope)) as UInt32 != 0
	}

	/// Returns the latency
	/// - remark: This corresponds to the property `kAudioDevicePropertyLatency`
	/// - parameter scope: The desired scope
	public func latency(in scope: PropertyScope) throws -> UInt32 {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyLatency), scope: scope))
	}

	/// Returns the device's streams
	/// - remark: This corresponds to the property `kAudioDevicePropertyStreams`
	/// - parameter scope: The desired scope
	public func streams(in scope: PropertyScope) throws -> [AudioStream] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStreams), scope: scope)).map { AudioObject.make($0) as! AudioStream }
	}

	/// Returns the device's audio controls
	/// - remark: This corresponds to the property `kAudioObjectPropertyControlList`
	/// - parameter scope: The desired scope
	public func controlList(in scope: PropertyScope) throws -> [AudioControl] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioObjectPropertyControlList), scope: scope)).map { AudioObject.make($0) as! AudioControl }
	}

	/// Returns the safety offset
	/// - remark: This corresponds to the property `kAudioDevicePropertySafetyOffset`
	/// - parameter scope: The desired scope
	public func safetyOffset(in scope: PropertyScope) throws -> UInt32 {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySafetyOffset), scope: scope))
	}

	/// Returns the sample rate
	/// - remark: This corresponds to the property `kAudioDevicePropertyNominalSampleRate`
	public func sampleRate() throws -> Double {
		return try getProperty(PropertyAddress(kAudioDevicePropertyNominalSampleRate))
	}
	/// Sets the sample rate
	/// - remark: This corresponds to the property `kAudioDevicePropertyNominalSampleRate`
	/// - parameter value: The desired property value
	public func setSampleRate(_ value: Double) throws {
		os_log(.info, log: audioObjectLog, "Setting device 0x%x sample rate to %.2f Hz", objectID, value)
		try setProperty(PropertyAddress(kAudioDevicePropertyNominalSampleRate), to: value)
	}

	/// Returns the available sample rates
	/// - remark: This corresponds to the property `kAudioDevicePropertyAvailableNominalSampleRates`
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try getProperty(PropertyAddress(kAudioDevicePropertyAvailableNominalSampleRates))
	}

	/// Returns the URL of the device's icon
	/// - remark: This corresponds to the property `kAudioDevicePropertyIcon`
	public func icon() throws -> URL {
		return try getProperty(PropertyAddress(kAudioDevicePropertyIcon))
	}

	/// Returns `true` if the device is hidden
	/// - remark: This corresponds to the property `kAudioDevicePropertyIsHidden`
	public func isHidden() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyIsHidden)) as UInt32 != 0
	}

	/// Returns the preferred stereo channels for the device
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelsForStereo`
	/// - parameter scope: The desired scope
	public func preferredStereoChannels(in scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), scope: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}

	/// Sets the preferred stereo channels
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelsForStereo`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	public func setPreferredStereoChannels(_ value: (UInt32, UInt32), scope: PropertyScope) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), scope: scope), to: [value.0, value.1])
	}

	/// Returns the preferred channel layout
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelLayout`
	/// - parameter scope: The desired scope
	public func preferredChannelLayout(in scope: PropertyScope) throws -> AudioChannelLayoutWrapper {
		let property = PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelLayout), scope: scope)
		let dataSize = try audioObjectPropertySize(property, from: objectID)
		let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: dataSize)
		defer {
			mem.deallocate()
		}
		try readAudioObjectProperty(property, from: objectID, into: mem, size: dataSize)
		return AudioChannelLayoutWrapper(mem)
	}
	/// Sets the preferred channel layout
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelLayout`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	public func setPreferredChannelLayout(_ value: UnsafePointer<AudioChannelLayout>, in scope: PropertyScope) throws {
		let dataSize = AudioChannelLayout.sizeInBytes(maximumDescriptions: Int(value.pointee.mNumberChannelDescriptions))
		try writeAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelLayout), scope: scope), on: objectID, from: value, size: dataSize)
	}
}

// MARK: - Audio Device Properties

extension AudioDevice {
	/// Returns any error codes loading the driver plugin
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlugIn`
	public func plugIn() throws -> OSStatus {
		return try getProperty(PropertyAddress(kAudioDevicePropertyPlugIn))
	}

	/// Returns `true` if the device is running somewhere
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceIsRunningSomewhere`
	public func isRunningSomewhere() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceIsRunningSomewhere)) as UInt32 != 0
	}

	/// Returns the owning pid or `-1` if the device is available to all processes
	/// - remark: This corresponds to the property `kAudioDevicePropertyHogMode`
	public func hogMode() throws -> pid_t {
		return try getProperty(PropertyAddress(kAudioDevicePropertyHogMode))
	}
	/// Sets the owning pid
	/// - remark: This corresponds to the property `kAudioDevicePropertyHogMode`
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

	/// Returns the buffer size in frames
	/// - remark: This corresponds to the property `kAudioDevicePropertyBufferFrameSize`
	public func bufferFrameSize() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioDevicePropertyBufferFrameSize))
	}
	/// Sets the buffer size in frames
	/// - remark: This corresponds to the property `kAudioDevicePropertyBufferFrameSize`
	public func setBufferFrameSize(_ value: UInt32) throws {
		try setProperty(PropertyAddress(kAudioDevicePropertyBufferFrameSize), to: value)
	}

	/// Returns the minimum and maximum values for the buffer size in frames
	/// - remark: This corresponds to the property `kAudioDevicePropertyBufferFrameSizeRange`
	public func bufferFrameSizeRange() throws -> AudioValueRange {
		return try getProperty(PropertyAddress(kAudioDevicePropertyBufferFrameSizeRange))
	}

	/// Returns the variable buffer frame size
	/// - remark: This corresponds to the property `kAudioDevicePropertyUsesVariableBufferFrameSizes`
	public func usesVariableBufferFrameSizes() throws -> UInt32 {
		return try getProperty(PropertyAddress(kAudioDevicePropertyHogMode))
	}

	/// Returns the IO cycle usage
	/// - remark: This corresponds to the property `kAudioDevicePropertyIOCycleUsage`
	public func ioCycleUsage() throws -> Float {
		return try getProperty(PropertyAddress(kAudioDevicePropertyHogMode))
	}

	/// Returns the stream configuration
	/// - remark: This corresponds to the property `kAudioDevicePropertyStreamConfiguration`
	public func streamConfiguration(in scope: PropertyScope) throws -> AudioBufferListWrapper {
		let property = PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStreamConfiguration), scope: scope)
		let dataSize = try audioObjectPropertySize(property, from: objectID)
		let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: dataSize)
		defer {
			mem.deallocate()
		}
		try readAudioObjectProperty(property, from: objectID, into: mem, size: dataSize)
		return AudioBufferListWrapper(mem)
	}

	/// Returns IOProc stream usage
	/// - note: This corresponds to `kAudioDevicePropertyIOProcStreamUsage`
	/// - parameter ioProc: The desired IOProc
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
//	public func ioProcStreamUsage(_ ioProc: UnsafeMutableRawPointer, in scope: PropertyScope) throws -> AudioHardwareIOProcStreamUsageWrapper {
//		return try __ioProcStreamUsage(ioProc, scope: scope)
//	}

	/// Returns the actual sample rate
	/// - remark: This corresponds to the property `kAudioDevicePropertyActualSampleRate`
	public func actualSampleRate() throws -> Double {
		return try getProperty(PropertyAddress(kAudioDevicePropertyActualSampleRate))
	}

	/// Returns the UID of the clock device
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockDevice`
	public func clockDevice() throws -> String {
		return try getProperty(PropertyAddress(kAudioDevicePropertyClockDevice))
	}

	/// Returns the workgroup to which the device's IOThread belongs
	/// - remark: This corresponds to the property `kAudioDevicePropertyIOThreadOSWorkgroup`
	@available(macOS 11.0, *)
	public func ioThreadOSWorkgroup(in scope: PropertyScope = .global) throws -> WorkGroup {
		var value: WorkGroup = unsafeBitCast(0, to: WorkGroup.self)
		try readAudioObjectProperty(PropertyAddress(kAudioDevicePropertyIOThreadOSWorkgroup), from: objectID, into: &value)
		return value
	}
}

// MARK: - Audio Device Properties Implemented by Audio Controls

extension AudioDevice {
	/// Returns `true` if a jack is connected to `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyJackIsConnected`
	public func jackIsConnected(to element: PropertyElement = .master, in scope: PropertyScope = .global) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyJackIsConnected), scope: scope, element: element)) as UInt32 != 0
	}

	/// Returns the volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeScalar`
	public func volumeScalar(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeScalar), scope: scope, element: channel))
	}
	/// Sets the volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeScalar`
	public func setVolumeScalar(_ value: Float, channel: PropertyElement = .master, in scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeScalar), scope: scope, element: channel), to: value)
	}

	/// Returns the volume decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibels`
	public func volumeDecibels(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeDecibels), scope: scope, element: channel))
	}
	/// Sets the volume decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibels`
	public func setVolumeDecibels(_ value: Float, channel: PropertyElement = .master, scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeDecibels), scope: scope, element: channel), to: value)
	}

	/// Returns the volume range in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeRangeDecibels`
	public func volumeRangeDecibels(_ channel: PropertyElement = .master, scope: PropertyScope = .global) throws -> AudioValueRange {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeRangeDecibels), scope: scope, element: channel))
	}

	/// Returns the stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPan`
	public func stereoPan(in scope: PropertyScope) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPan), scope: scope))
	}
	/// Sets the stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPan`
	public func setStereoPan(_ value: Float, in scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPan), scope: scope), to: value)
	}

	/// Returns the channels used for stereo panning
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPanChannels`
	public func stereoPanChannels(in scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPanChannels), scope: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}
	/// Sets the channels used for stereo panning
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPanChannels`
	public func setStereoPanChannels(_ value: (UInt32, UInt32), scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPanChannels), scope: scope), to: [value.0, value.1])
	}

	/// Returns `true` if `element` is muted
	/// - remark: This corresponds to the property `kAudioDevicePropertyMute`
	public func mute(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyMute), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether `element` is muted
	/// - remark: This corresponds to the property `kAudioDevicePropertyMute`
	public func setMute(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyMute), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if only `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertySolo`
	public func solo(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySolo), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertySolo`
	public func setSolo(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySolo), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if phantom power is enabled for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhantomPower`
	public func phantomPower(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhantomPower), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether phantom power is enabled for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhantomPower`
	public func setPhantomPower(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhantomPower), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the phase is inverted for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhaseInvert`
	public func phaseInvert(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhaseInvert), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether the phase is inverted for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhaseInvert`
	public func setPhaseInvert(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhaseInvert), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the signal exceeded the sample range
	/// - remark: This corresponds to the property `kAudioDevicePropertyClipLight`
	public func clipLight(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClipLight), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether the signal exceeded the sample range
	/// - remark: This corresponds to the property `kAudioDevicePropertyClipLight`
	public func setClipLight(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClipLight), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if talkback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyTalkback`
	public func talkback(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyTalkback), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether talkback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyTalkback`
	public func setTalkback(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyTalkback), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if listenback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyListenback`
	public func listenback(in scope: PropertyScope, on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyListenback), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether listenback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyListenback`
	public func setListenback(_ value: Bool, in scope: PropertyScope, on element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyListenback), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns the IDs of all the currently selected data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSource`
	public func dataSource(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSource), scope: scope))
	}

	/// Sets the currently selected data sources (`kAudioDevicePropertyDataSource` )
	public func setDataSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSource), scope: scope), to: value)
	}

	/// Returns the IDs of all the currently available data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSources`
	public func dataSources(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSources), scope: scope))
	}

	/// Returns the name of `dataSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSourceNameForIDCFString`
	public func dataSourceName(_ dataSourceID: UInt32, scope: PropertyScope) throws -> String {
		var inputData = dataSourceID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSourceNameForIDCFString), scope: scope), from: objectID, into: &translation)
			}
		}
		return outputData as String
	}

	/// Returns the kind of `dataSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSourceKindForID`
	public func dataSourceKind(_ dataSourceID: UInt32, scope: PropertyScope) throws -> UInt32 {
		var inputData = dataSourceID
		var outputData: UInt32 = 0
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<UInt32>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSourceKindForID), scope: scope), from: objectID, into: &translation)
			}
		}
		return outputData
	}

	// Data source helpers

	/// Returns the available data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSources`
	public func availableDataSources(in scope: PropertyScope) throws -> [DataSource] {
		return try dataSources(in: scope).map { DataSource(device: self, scope: scope, id: $0) }
	}

	/// Returns the active  data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSource`
	public func activeDataSources(in scope: PropertyScope) throws -> [DataSource] {
		return try dataSource(in: scope).map { DataSource(device: self, scope: scope, id: $0) }
	}

	/// Returns the IDs of all the currently selected clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func clockSource(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSource), scope: scope))
	}

	/// Sets the currently selected clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func setClockSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSource), scope: scope), to: value)
	}

	/// Returns the IDs of all the currently available clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSources`
	public func clockSources(in scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSources), scope: scope))
	}

	/// Returns the name of `clockSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSourceNameForIDCFString`
	public func clockSourceName(_ clockSourceID: UInt32, scope: PropertyScope) throws -> String {
		var inputData = clockSourceID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSourceNameForIDCFString), scope: scope), from: objectID, into: &translation)
			}
		}
		return outputData as String
	}

	/// Returns the kind of `clockSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSourceKindForID`
	/// - parameter clockSourceID: The desired clock source
	/// - parameter scope: The desired scope
	/// - throws: An error if the property could not be retrieved
	public func clockSourceKind(_ clockSourceID: UInt32, scope: PropertyScope) throws -> UInt32 {
		var inputData = clockSourceID
		var outputData: UInt32 = 0
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<UInt32>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSourceKindForID), scope: scope), from: objectID, into: &translation)
			}
		}
		return outputData
	}

	// Clock source helpers

	/// Returns the available clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSources`
	public func availableClockSources(in scope: PropertyScope) throws -> [ClockSource] {
		return try clockSources(in: scope).map { ClockSource(device: self, scope: scope, id: $0) }
	}

	/// Returns the active  clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func activeClockSources(in scope: PropertyScope) throws -> [ClockSource] {
		return try clockSource(in: scope).map { ClockSource(device: self, scope: scope, id: $0) }
	}

	/// Returns `true` if play through is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThru`
	public func playThru(on element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPlayThru), scope: .playThrough, element: element)) as UInt32 != 0
	}

//	/// Returns `true` if only the specified play through element is audible
//	/// - note: This corresponds to the property `kAudioDevicePropertyPlayThruSolo`
//	/// - parameter scope: The desired scope
//	/// - parameter element: The desired element
//	public func playThruSolo(on element: PropertyElement = .master) throws -> Bool {
//		return try getProperty(.devicePlayThruSolo, scope: .playThrough, element: element) as UInt32 != 0
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
		public let device: AudioDevice
		/// Returns the data source scope
		public let scope: PropertyScope
		/// Returns the data source ID
		public let id: UInt32

		/// Returns the data source name
		public func name() throws -> String {
			return try device.dataSourceName(id, scope: scope)
		}

		/// Returns the data source kind
		public func kind() throws -> UInt32 {
			return try device.dataSourceKind(id, scope: scope)
		}
	}
}

extension AudioDevice.DataSource: CustomDebugStringConvertible {
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)): (\(scope), '\(id.fourCC)') \"\(name)\" on AudioDevice 0x\(String(device.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)): (\(scope), '\(id.fourCC)') on AudioDevice 0x\(String(device.objectID, radix: 16, uppercase: false)))>"
		}
	}
}

extension AudioDevice {
	/// A clock source for an audio device
	public struct ClockSource {
		/// Returns the owning audio device
		public let device: AudioDevice
		/// Returns the clock source scope
		public let scope: PropertyScope
		/// Returns the clock source ID
		public let id: UInt32

		/// Returns the clock source name
		public func name() throws -> String {
			return try device.clockSourceName(id, scope: scope)
		}

		/// Returns the clock source kind
		public func kind() throws -> UInt32 {
			return try device.clockSourceKind(id, scope: scope)
		}
	}
}

extension AudioDevice.ClockSource: CustomDebugStringConvertible {
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)): (\(scope), '\(id.fourCC)') \"\(name)\" on AudioDevice 0x\(String(device.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)): (\(scope), '\(id.fourCC)') on AudioDevice 0x\(String(device.objectID, radix: 16, uppercase: false)))>"
		}
	}
}
