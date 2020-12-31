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
	public class func makeDevice(forUID uid: String) throws -> AudioDevice? {
		var qualifier = uid as CFString
		let objectID: AudioObjectID = try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateUIDToDevice), qualifier: PropertyQualifier(&qualifier))
		guard objectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(objectID) as! AudioDevice)
	}

	/// Returns `true` if the device supports input
	///
	/// - note: A device supports input if it has buffers in `kAudioObjectPropertyScopeInput` for the property `kAudioDevicePropertyStreamConfiguration`
	public func supportsInput() throws -> Bool {
		try streamConfiguration(inScope: .input).numberBuffers > 0
	}

	/// Returns `true` if the device supports output
	///
	/// - note: A device supports output if it has buffers in `kAudioObjectPropertyScopeOutput` for the property `kAudioDevicePropertyStreamConfiguration`
	public func supportsOutput() throws -> Bool {
		try streamConfiguration(inScope: .output).numberBuffers > 0
	}

	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)) \"\(try name())\", \(try isAlive() ? try isRunning() ? "running" : "stopped" : "dead")>"
		}
		catch {
			return super.debugDescription
		}
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
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceIsAlive)) as UInt32 != 0
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
	public func canBeDefault(inScope scope: PropertyScope) throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyDeviceCanBeDefaultDevice)) as UInt32 != 0
	}

	/// Returns `true` if the device can be the system default device
	/// - remark: This corresponds to the property `kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`
	/// - parameter scope: The desired scope
	public func canBeSystemDefault(inScope scope: PropertyScope) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDeviceCanBeDefaultSystemDevice), scope: scope)) as UInt32 != 0
	}

	/// Returns the latency
	/// - remark: This corresponds to the property `kAudioDevicePropertyLatency`
	/// - parameter scope: The desired scope
	public func latency(inScope scope: PropertyScope) throws -> UInt32 {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyLatency), scope: scope))
	}

	/// Returns the device's streams
	/// - remark: This corresponds to the property `kAudioDevicePropertyStreams`
	/// - parameter scope: The desired scope
	public func streams(inScope scope: PropertyScope) throws -> [AudioStream] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStreams), scope: scope)).map { AudioObject.make($0) as! AudioStream }
	}

	/// Returns the device's audio controls
	/// - remark: This corresponds to the property `kAudioObjectPropertyControlList`
	public func controlList() throws -> [AudioControl] {
		return try getProperty(PropertyAddress(kAudioObjectPropertyControlList)).map { AudioObject.make($0) as! AudioControl }
	}

	/// Returns the safety offset
	/// - remark: This corresponds to the property `kAudioDevicePropertySafetyOffset`
	/// - parameter scope: The desired scope
	public func safetyOffset(inScope scope: PropertyScope) throws -> UInt32 {
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
	public func availableSampleRates() throws -> [ClosedRange<Double>] {
		let value: [AudioValueRange] = try getProperty(PropertyAddress(kAudioDevicePropertyAvailableNominalSampleRates))
		return value.map { $0.mMinimum ... $0.mMaximum }
	}

	/// Returns the URL of the device's icon
	/// - remark: This corresponds to the property `kAudioDevicePropertyIcon`
	public func icon() throws -> URL {
		var value: CFTypeRef = unsafeBitCast(0, to: CFTypeRef.self)
		try readAudioObjectProperty(PropertyAddress(kAudioDevicePropertyIcon), from: objectID, into: &value)
		return value as! URL
	}

	/// Returns `true` if the device is hidden
	/// - remark: This corresponds to the property `kAudioDevicePropertyIsHidden`
	public func isHidden() throws -> Bool {
		return try getProperty(PropertyAddress(kAudioDevicePropertyIsHidden)) as UInt32 != 0
	}

	/// Returns the preferred stereo channels for the device
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelsForStereo`
	/// - parameter scope: The desired scope
	public func preferredStereoChannels(inScope scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), scope: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}
	/// Sets the preferred stereo channels
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelsForStereo`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	public func setPreferredStereoChannels(_ value: (UInt32, UInt32), inScope scope: PropertyScope) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), scope: scope), to: [value.0, value.1])
	}

	/// Returns the preferred channel layout
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelLayout`
	/// - parameter scope: The desired scope
	public func preferredChannelLayout(inScope scope: PropertyScope) throws -> AudioChannelLayoutWrapper {
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
	public func setPreferredChannelLayout(_ value: UnsafePointer<AudioChannelLayout>, inScope scope: PropertyScope) throws {
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
	public func bufferFrameSizeRange() throws -> ClosedRange<UInt32> {
		let value: AudioValueRange = try getProperty(PropertyAddress(kAudioDevicePropertyBufferFrameSizeRange))
		return UInt32(value.mMinimum) ... UInt32(value.mMaximum)
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
	public func streamConfiguration(inScope scope: PropertyScope) throws -> AudioBufferListWrapper {
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
//	public func ioProcStreamUsage(_ ioProc: UnsafeMutableRawPointer, inScope scope: PropertyScope) throws -> AudioHardwareIOProcStreamUsageWrapper {
//		let property = PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyIOProcStreamUsage), scope: scope)
//		let dataSize = try audioObjectPropertySize(property, from: objectID)
//		let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: dataSize)
//		defer {
//			mem.deallocate()
//		}
//		UnsafeMutableRawPointer(mem).assumingMemoryBound(to: AudioHardwareIOProcStreamUsage.self).pointee.mIOProc = ioProc
//		try readAudioObjectProperty(property, from: objectID, into: mem, size: dataSize)
//		return AudioHardwareIOProcStreamUsageWrapper(mem)
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
	public func ioThreadOSWorkgroup(inScope scope: PropertyScope = .global) throws -> WorkGroup {
		var value: WorkGroup = unsafeBitCast(0, to: WorkGroup.self)
		try readAudioObjectProperty(PropertyAddress(kAudioDevicePropertyIOThreadOSWorkgroup), from: objectID, into: &value)
		return value
	}
}

// MARK: - Audio Device Properties Implemented by Audio Controls

extension AudioDevice {
	/// Returns `true` if a jack is connected to `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyJackIsConnected`
	public func jackIsConnected(toElement element: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyJackIsConnected), scope: scope, element: element)) as UInt32 != 0
	}

	/// Returns the volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeScalar`
	public func volumeScalar(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeScalar), scope: scope, element: channel))
	}
	/// Sets the volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeScalar`
	public func setVolumeScalar(_ value: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeScalar), scope: scope, element: channel), to: value)
	}

	/// Returns the volume decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibels`
	public func volumeDecibels(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeDecibels), scope: scope, element: channel))
	}
	/// Sets the volume decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibels`
	public func setVolumeDecibels(_ value: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeDecibels), scope: scope, element: channel), to: value)
	}

	/// Returns the volume range in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeRangeDecibels`
	public func volumeRangeDecibels(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> ClosedRange<Float> {
		let value: AudioValueRange = try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyVolumeRangeDecibels), scope: scope, element: channel))
		return Float(value.mMinimum) ... Float(value.mMaximum)
	}

	/// Returns the stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPan`
	public func stereoPan(inScope scope: PropertyScope) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPan), scope: scope))
	}
	/// Sets the stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPan`
	public func setStereoPan(_ value: Float, inScope scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyStereoPan), scope: scope), to: value)
	}

	/// Returns the channels used for stereo panning
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPanChannels`
	public func stereoPanChannels(inScope scope: PropertyScope) throws -> (UInt32, UInt32) {
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
	public func mute(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyMute), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether `element` is muted
	/// - remark: This corresponds to the property `kAudioDevicePropertyMute`
	public func setMute(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyMute), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if only `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertySolo`
	public func solo(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySolo), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertySolo`
	public func setSolo(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertySolo), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if phantom power is enabled for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhantomPower`
	public func phantomPower(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhantomPower), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether phantom power is enabled for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhantomPower`
	public func setPhantomPower(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhantomPower), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the phase is inverted for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhaseInvert`
	public func phaseInvert(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhaseInvert), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether the phase is inverted for `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhaseInvert`
	public func setPhaseInvert(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPhaseInvert), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the signal exceeded the sample range
	/// - remark: This corresponds to the property `kAudioDevicePropertyClipLight`
	public func clipLight(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClipLight), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether the signal exceeded the sample range
	/// - remark: This corresponds to the property `kAudioDevicePropertyClipLight`
	public func setClipLight(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClipLight), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if talkback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyTalkback`
	public func talkback(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyTalkback), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether talkback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyTalkback`
	public func setTalkback(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyTalkback), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if listenback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyListenback`
	public func listenback(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyListenback), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether listenback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyListenback`
	public func setListenback(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyListenback), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns the IDs of all the currently selected data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSource`
	public func dataSource(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSource), scope: scope))
	}

	/// Sets the currently selected data sources (`kAudioDevicePropertyDataSource` )
	public func setDataSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSource), scope: scope), to: value)
	}

	/// Returns the IDs of all the currently available data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSources`
	public func dataSources(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyDataSources), scope: scope))
	}

	/// Returns the name of `dataSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSourceNameForIDCFString`
	public func nameOfDataSource(_ dataSourceID: UInt32, inScope scope: PropertyScope) throws -> String {
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
	public func kindOfDataSource(_ dataSourceID: UInt32, inScope scope: PropertyScope) throws -> UInt32 {
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
	public func availableDataSources(inScope scope: PropertyScope) throws -> [DataSource] {
		return try dataSources(inScope: scope).map { DataSource(device: self, scope: scope, id: $0) }
	}

	/// Returns the active  data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSource`
	public func activeDataSources(inScope scope: PropertyScope) throws -> [DataSource] {
		return try dataSource(inScope: scope).map { DataSource(device: self, scope: scope, id: $0) }
	}

	/// Returns the IDs of all the currently selected clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func clockSource(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSource), scope: scope))
	}

	/// Sets the currently selected clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func setClockSource(_ value: [UInt32], inScope scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSource), scope: scope), to: value)
	}

	/// Returns the IDs of all the currently available clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSources`
	public func clockSources(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyClockSources), scope: scope))
	}

	/// Returns the name of `clockSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSourceNameForIDCFString`
	public func nameOfClockSource(_ clockSourceID: UInt32, inScope scope: PropertyScope) throws -> String {
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
	public func kindOfClockSource(_ clockSourceID: UInt32, inScope scope: PropertyScope) throws -> UInt32 {
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
	public func availableClockSources(inScope scope: PropertyScope) throws -> [ClockSource] {
		return try clockSources(inScope: scope).map { ClockSource(device: self, scope: scope, id: $0) }
	}

	/// Returns the active  clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func activeClockSources(inScope scope: PropertyScope) throws -> [ClockSource] {
		return try clockSource(inScope: scope).map { ClockSource(device: self, scope: scope, id: $0) }
	}

	/// Returns `true` if play through is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThru`
	public func playThru(onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPlayThru), scope: .playThrough, element: element)) as UInt32 != 0
	}

	/// Returns `true` if only the specified play through element is audible
	/// - note: This corresponds to the property `kAudioDevicePropertyPlayThruSolo`
	public func playThruSolo(onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(rawValue: kAudioDevicePropertyPlayThruSolo), scope: .playThrough, element: element)) as UInt32 != 0
	}

/*
	public var kAudioDevicePropertyPlayThruVolumeScalar: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyPlayThruVolumeDecibels: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyPlayThruVolumeRangeDecibels: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyPlayThruVolumeScalarToDecibels: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyPlayThruVolumeDecibelsToScalar: AudioObjectPropertySelector { get }

	public var kAudioDevicePropertyPlayThruStereoPan: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyPlayThruStereoPanChannels: AudioObjectPropertySelector { get }

	public var kAudioDevicePropertyPlayThruDestination: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyPlayThruDestinations: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyPlayThruDestinationNameForIDCFString: AudioObjectPropertySelector { get }

	public var kAudioDevicePropertyChannelNominalLineLevel: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyChannelNominalLineLevels: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyChannelNominalLineLevelNameForIDCFString: AudioObjectPropertySelector { get }

	public var kAudioDevicePropertyHighPassFilterSetting: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyHighPassFilterSettings: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertyHighPassFilterSettingNameForIDCFString: AudioObjectPropertySelector { get }

	public var kAudioDevicePropertySubVolumeScalar: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertySubVolumeDecibels: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertySubVolumeRangeDecibels: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertySubVolumeScalarToDecibels: AudioObjectPropertySelector { get }
	public var kAudioDevicePropertySubVolumeDecibelsToScalar: AudioObjectPropertySelector { get }

	public var kAudioDevicePropertySubMute: AudioObjectPropertySelector { get }
	*/

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
			return try device.nameOfDataSource(id, inScope: scope)
		}

		/// Returns the data source kind
		public func kind() throws -> UInt32 {
			return try device.kindOfDataSource(id, inScope: scope)
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
			return try device.nameOfClockSource(id, inScope: scope)
		}

		/// Returns the clock source kind
		public func kind() throws -> UInt32 {
			return try device.kindOfClockSource(id, inScope: scope)
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
