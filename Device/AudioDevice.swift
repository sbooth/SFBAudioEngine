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
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)) \"\(try name())\">"
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
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDeviceCanBeDefaultSystemDevice), scope: scope)) as UInt32 != 0
	}

	/// Returns the latency
	/// - remark: This corresponds to the property `kAudioDevicePropertyLatency`
	/// - parameter scope: The desired scope
	public func latency(inScope scope: PropertyScope) throws -> UInt32 {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyLatency), scope: scope))
	}

	/// Returns the device's streams
	/// - remark: This corresponds to the property `kAudioDevicePropertyStreams`
	/// - parameter scope: The desired scope
	public func streams(inScope scope: PropertyScope) throws -> [AudioStream] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyStreams), scope: scope)).map { AudioObject.make($0) as! AudioStream }
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
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySafetyOffset), scope: scope))
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
		var value: CFTypeRef! = nil
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
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPreferredChannelsForStereo), scope: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}
	/// Sets the preferred stereo channels
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelsForStereo`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	public func setPreferredStereoChannels(_ value: (UInt32, UInt32), inScope scope: PropertyScope) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPreferredChannelsForStereo), scope: scope), to: [value.0, value.1])
	}

	/// Returns the preferred channel layout
	/// - remark: This corresponds to the property `kAudioDevicePropertyPreferredChannelLayout`
	/// - parameter scope: The desired scope
	public func preferredChannelLayout(inScope scope: PropertyScope) throws -> AudioChannelLayoutWrapper {
		let property = PropertyAddress(PropertySelector(kAudioDevicePropertyPreferredChannelLayout), scope: scope)
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
		try writeAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPreferredChannelLayout), scope: scope), on: objectID, from: value, size: dataSize)
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
		let property = PropertyAddress(PropertySelector(kAudioDevicePropertyStreamConfiguration), scope: scope)
		let dataSize = try audioObjectPropertySize(property, from: objectID)
		let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: dataSize)
		defer {
			mem.deallocate()
		}
		try readAudioObjectProperty(property, from: objectID, into: mem, size: dataSize)
		return AudioBufferListWrapper(mem)
	}

	/// Returns IOProc stream usage
	/// - note: This corresponds to the property `kAudioDevicePropertyIOProcStreamUsage`
	/// - parameter ioProc: The desired IOProc
	public func ioProcStreamUsage(_ ioProc: UnsafeMutableRawPointer, inScope scope: PropertyScope) throws -> AudioHardwareIOProcStreamUsageWrapper {
		let property = PropertyAddress(PropertySelector(kAudioDevicePropertyIOProcStreamUsage), scope: scope)
		let dataSize = try audioObjectPropertySize(property, from: objectID)
		let mem = UnsafeMutablePointer<UInt8>.allocate(capacity: dataSize)
		defer {
			mem.deallocate()
		}
		UnsafeMutableRawPointer(mem).assumingMemoryBound(to: AudioHardwareIOProcStreamUsage.self).pointee.mIOProc = ioProc
		try readAudioObjectProperty(property, from: objectID, into: mem, size: dataSize)
		return AudioHardwareIOProcStreamUsageWrapper(mem)
	}
	/// Sets IOProc stream usage
	/// - note: This corresponds to the property `kAudioDevicePropertyIOProcStreamUsage`
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	public func setIOProcStreamUsage(_ value: UnsafePointer<AudioHardwareIOProcStreamUsage>, inScope scope: PropertyScope) throws {
		let dataSize = AudioHardwareIOProcStreamUsage.sizeInBytes(maximumStreams: Int(value.pointee.mNumberStreams))
		try writeAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyIOProcStreamUsage), scope: scope), on: objectID, from: value, size: dataSize)
	}

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
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyJackIsConnected), scope: scope, element: element)) as UInt32 != 0
	}

	// It would be possible to combine the kAudioDevicePropertyVolume* and kAudioDevicePropertyPlayThruVolume* properties
	// in the following methods based on the scope, choosing the kAudioDevicePropertyPlayThruVolume* variants when scope is
	// kAudioObjectPropertyScopePlayThrough and the kAudioDevicePropertyVolume* properties otherwise. However, it's unclear
	// (to me at least) whether kAudioDevicePropertyPlayThruVolumeScalar, for example, could have a meaning in the
	// kAudioObjectPropertyScopePlayThrough scope. If it could then combining the two sets of properties here would not
	// allow the kAudioDevicePropertyVolume* properties to be set in the kAudioObjectPropertyScopePlayThrough scope.
	// For this reason the kAudioDevicePropertyPlayThruVolume* are given their own methods.

	/// Returns the volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeScalar`
	public func volumeScalar(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyVolumeScalar), scope: scope, element: channel))
	}
	/// Sets the volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeScalar`
	public func setVolumeScalar(_ value: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyVolumeScalar), scope: scope, element: channel), to: value)
	}

	/// Returns the volume in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibels`
	public func volumeDecibels(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyVolumeDecibels), scope: scope, element: channel))
	}
	/// Sets the volume in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibels`
	public func setVolumeDecibels(_ value: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyVolumeDecibels), scope: scope, element: channel), to: value)
	}

	/// Returns the volume range in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeRangeDecibels`
	public func volumeRangeDecibels(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> ClosedRange<Float> {
		let value: AudioValueRange = try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyVolumeRangeDecibels), scope: scope, element: channel))
		return Float(value.mMinimum) ... Float(value.mMaximum)
	}

	/// Converts volume `scalar` to decibels and returns the converted value
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeScalarToDecibels`
	/// - parameter scalar: The value to convert
	public func convertVolumeToDecibels(fromScalar scalar: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyVolumeScalarToDecibels), scope: scope, element: channel), initialValue: scalar)
	}

	/// Converts volume `decibels` to scalar and returns the converted value
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibelsToScalar`
	/// - parameter decibels: The value to convert
	public func convertVolumeToScalar(fromDecibels decibels: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyVolumeDecibelsToScalar), scope: scope, element: channel), initialValue: decibels)
	}

	/// Returns the stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPan`
	public func stereoPan(inScope scope: PropertyScope) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyStereoPan), scope: scope))
	}
	/// Sets the stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPan`
	public func setStereoPan(_ value: Float, inScope scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyStereoPan), scope: scope), to: value)
	}

	/// Returns the channels used for stereo panning
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPanChannels`
	public func stereoPanChannels(inScope scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyStereoPanChannels), scope: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}
	/// Sets the channels used for stereo panning
	/// - remark: This corresponds to the property `kAudioDevicePropertyStereoPanChannels`
	public func setStereoPanChannels(_ value: (UInt32, UInt32), inScope scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyStereoPanChannels), scope: scope), to: [value.0, value.1])
	}

	/// Returns `true` if `element` is muted
	/// - remark: This corresponds to the property `kAudioDevicePropertyMute`
	public func mute(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyMute), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether `element` is muted
	/// - remark: This corresponds to the property `kAudioDevicePropertyMute`
	public func setMute(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyMute), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if only `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertySolo`
	public func solo(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySolo), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertySolo`
	public func setSolo(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySolo), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if phantom power is enabled on `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhantomPower`
	public func phantomPower(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPhantomPower), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether phantom power is enabled on `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhantomPower`
	public func setPhantomPower(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPhantomPower), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the phase is inverted on `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhaseInvert`
	public func phaseInvert(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPhaseInvert), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether the phase is inverted on `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPhaseInvert`
	public func setPhaseInvert(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPhaseInvert), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if the signal exceeded the sample range
	/// - remark: This corresponds to the property `kAudioDevicePropertyClipLight`
	public func clipLight(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyClipLight), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether the signal exceeded the sample range
	/// - remark: This corresponds to the property `kAudioDevicePropertyClipLight`
	public func setClipLight(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyClipLight), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if talkback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyTalkback`
	public func talkback(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyTalkback), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether talkback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyTalkback`
	public func setTalkback(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyTalkback), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns `true` if listenback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyListenback`
	public func listenback(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyListenback), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether listenback is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyListenback`
	public func setListenback(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyListenback), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns the IDs of the selected data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSource`
	public func dataSource(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDataSource), scope: scope))
	}
	/// Sets the IDs of the selected data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSource`
	public func setDataSource(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDataSource), scope: scope), to: value)
	}

	/// Returns the IDs of the available data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSources`
	public func dataSources(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDataSources), scope: scope))
	}

	/// Returns the name of `dataSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSourceNameForIDCFString`
	public func nameOfDataSource(_ dataSourceID: UInt32, inScope scope: PropertyScope) throws -> String {
		var inputData = dataSourceID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDataSourceNameForIDCFString), scope: scope), from: objectID, into: &translation)
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
				try readAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDataSourceKindForID), scope: scope), from: objectID, into: &translation)
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

	/// Returns the active data sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyDataSource`
	public func activeDataSources(inScope scope: PropertyScope) throws -> [DataSource] {
		return try dataSource(inScope: scope).map { DataSource(device: self, scope: scope, id: $0) }
	}

	/// Returns the IDs of the selected clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func clockSource(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyClockSource), scope: scope))
	}
	/// Sets the IDs of the selected clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func setClockSource(_ value: [UInt32], inScope scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyClockSource), scope: scope), to: value)
	}

	/// Returns the IDs of the available clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSources`
	public func clockSources(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyClockSources), scope: scope))
	}

	/// Returns the name of `clockSourceID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSourceNameForIDCFString`
	public func nameOfClockSource(_ clockSourceID: UInt32, inScope scope: PropertyScope) throws -> String {
		var inputData = clockSourceID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyClockSourceNameForIDCFString), scope: scope), from: objectID, into: &translation)
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
				try readAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyClockSourceKindForID), scope: scope), from: objectID, into: &translation)
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

	/// Returns the active clock sources
	/// - remark: This corresponds to the property `kAudioDevicePropertyClockSource`
	public func activeClockSources(inScope scope: PropertyScope) throws -> [ClockSource] {
		return try clockSource(inScope: scope).map { ClockSource(device: self, scope: scope, id: $0) }
	}

	/// Returns `true` if play-through is enabled
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThru`
	public func playThrough(onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThru), scope: .playThrough, element: element)) as UInt32 != 0
	}

	/// Returns `true` if only play-through `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruSolo`
	public func playThroughSolo(onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruSolo), scope: .playThrough, element: element)) as UInt32 != 0
	}
	/// Sets whether play-through `element` is audible
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruSolo`
	public func setPlayThroughSolo(_ value: Bool, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruSolo), scope: .playThrough, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns the play-through volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeScalar`
	public func playThroughVolumeScalar(forChannel channel: PropertyElement = .master) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruVolumeScalar), scope: .playThrough, element: channel))
	}
	/// Sets the play-through volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeScalar`
	public func setPlayThroughVolumeScalar(_ value: Float, forChannel channel: PropertyElement = .master) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruVolumeScalar), scope: .playThrough, element: channel), to: value)
	}

	/// Returns the play-through volume in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeDecibels`
	public func playThroughVolumeDecibels(forChannel channel: PropertyElement = .master) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruVolumeDecibels), scope: .playThrough, element: channel))
	}
	/// Sets the play-through volume in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeDecibels`
	public func setPlayThroughVolumeDecibels(_ value: Float, forChannel channel: PropertyElement = .master) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruVolumeDecibels), scope: .playThrough, element: channel), to: value)
	}

	/// Returns the play-through volume range in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeRangeDecibels`
	public func playThroughVolumeRangeDecibels(forChannel channel: PropertyElement = .master) throws -> ClosedRange<Float> {
		let value: AudioValueRange = try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruVolumeRangeDecibels), scope: .playThrough, element: channel))
		return Float(value.mMinimum) ... Float(value.mMaximum)
	}

	/// Converts play-through volume `scalar` to decibels and returns the converted value
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeScalarToDecibels`
	/// - parameter scalar: The value to convert
	public func convertPlayThroughVolumeToDecibels(fromScalar scalar: Float, forChannel channel: PropertyElement = .master) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruVolumeScalarToDecibels), scope: .playThrough, element: channel), initialValue: scalar)
	}

	/// Converts play-through volume `decibels` to scalar and returns the converted value
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeDecibelsToScalar`
	/// - parameter decibels: The value to convert
	public func convertPlayThroughVolumeToScalar(fromDecibels decibels: Float, forChannel channel: PropertyElement = .master) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruVolumeDecibelsToScalar), scope: .playThrough, element: channel), initialValue: decibels)
	}

	/// Returns the play-through stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruStereoPan`
	public func playThroughStereoPan() throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruStereoPan), scope: .playThrough))
	}
	/// Sets the play-through stereo pan
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruStereoPan`
	public func setPlayThroughStereoPan(_ value: Float) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruStereoPan), scope: .playThrough), to: value)
	}

	/// Returns the play-through channels used for stereo panning
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruStereoPanChannels`
	public func playThroughStereoPanChannels() throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruStereoPanChannels), scope: .playThrough))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}
	/// Sets the play-through channels used for stereo panning
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruStereoPanChannels`
	public func setPlayThroughStereoPanChannels(_ value: (UInt32, UInt32)) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruStereoPanChannels), scope: .playThrough), to: [value.0, value.1])
	}

	/// Returns the IDs of the selected play-through destinations
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruDestination`
	public func playThroughDestination() throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruDestination), scope: .playThrough))
	}
	/// Sets the IDs of the selected play-through destinations
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruDestination`
	public func setPlayThroughDestination(_ value: [UInt32]) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruDestination), scope: .playThrough), to: value)
	}

	/// Returns the IDs of the available play-through destinations
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruDestinations`
	public func playThroughDestinations() throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruDestinations), scope: .playThrough))
	}

	/// Returns the name of `playThroughDestinationID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruDestinationNameForIDCFString`
	public func nameOfPlayThroughDestination(_ playThroughDestinationID: UInt32) throws -> String {
		var inputData = playThroughDestinationID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyPlayThruDestinationNameForIDCFString), scope: .playThrough), from: objectID, into: &translation)
			}
		}
		return outputData as String
	}

	// Play-through destination helpers

	/// Returns the available play-through destinations
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruDestinations`
	public func availablePlayThroughDestinations() throws -> [PlayThroughDestination] {
		return try playThroughDestination().map { PlayThroughDestination(device: self, id: $0) }
	}

	/// Returns the selected play-through destinations
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruDestination`
	public func selectedPlayThroughDestinations() throws -> [PlayThroughDestination] {
		return try playThroughDestinations().map { PlayThroughDestination(device: self, id: $0) }
	}

	/// Returns the IDs of the selected channel nominal line levels
	/// - remark: This corresponds to the property `kAudioDevicePropertyChannelNominalLineLevel`
	public func channelNominalLineLevel(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyChannelNominalLineLevel), scope: scope))
	}
	/// Sets the IDs of the selected channel nominal line levels
	/// - remark: This corresponds to the property `kAudioDevicePropertyChannelNominalLineLevel`
	public func setChannelNominalLineLevel(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyChannelNominalLineLevel), scope: scope), to: value)
	}

	/// Returns the IDs of the available channel nominal line levels
	/// - remark: This corresponds to the property `kAudioDevicePropertyChannelNominalLineLevels`
	public func channelNominalLineLevels(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyChannelNominalLineLevels), scope: scope))
	}

	/// Returns the name of `channelNominalLineLevelID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyChannelNominalLineLevelNameForIDCFString`
	public func nameOfChannelNominalLineLevel(_ channelNominalLineLevelID: UInt32, inScope scope: PropertyScope) throws -> String {
		var inputData = channelNominalLineLevelID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyChannelNominalLineLevelNameForIDCFString), scope: scope), from: objectID, into: &translation)
			}
		}
		return outputData as String
	}

	// Channel nominal line level helpers

	/// Returns the available channel nominal line levels
	/// - remark: This corresponds to the property `kAudioDevicePropertyChannelNominalLineLevels`
	public func availableChannelNominalLineLevels(inScope scope: PropertyScope) throws -> [ChannelNominalLineLevel] {
		return try channelNominalLineLevel(inScope: scope).map { ChannelNominalLineLevel(device: self, scope: scope, id: $0) }
	}

	/// Returns the selected channel nominal line levels
	/// - remark: This corresponds to the property `kAudioDevicePropertyChannelNominalLineLevel`
	public func selectedChannelNominalLineLevels(inScope scope: PropertyScope) throws -> [ChannelNominalLineLevel] {
		return try channelNominalLineLevels(inScope: scope).map { ChannelNominalLineLevel(device: self, scope: scope, id: $0) }
	}

	/// Returns the IDs of the selected high-pass filter settings
	/// - remark: This corresponds to the property `kAudioDevicePropertyHighPassFilterSetting`
	public func highPassFilterSetting(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyHighPassFilterSetting), scope: scope))
	}
	/// Sets the IDs of the selected high-pass filter settings
	/// - remark: This corresponds to the property `kAudioDevicePropertyHighPassFilterSetting`
	public func setHighPassFilterSetting(_ value: [UInt32], scope: PropertyScope) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyHighPassFilterSetting), scope: scope), to: value)
	}

	/// Returns the IDs of the available high-pass filter settings
	/// - remark: This corresponds to the property `kAudioDevicePropertyHighPassFilterSettings`
	public func highPassFilterSettings(inScope scope: PropertyScope) throws -> [UInt32] {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyHighPassFilterSettings), scope: scope))
	}

	/// Returns the name of `highPassFilterSettingID`
	/// - remark: This corresponds to the property `kAudioDevicePropertyHighPassFilterSettingNameForIDCFString`
	public func nameOfHighPassFilterSetting(_ highPassFilterSettingID: UInt32, inScope scope: PropertyScope) throws -> String {
		var inputData = highPassFilterSettingID
		var outputData = unsafeBitCast(0, to: CFString.self)
		try withUnsafeMutablePointer(to: &inputData) { inputPointer in
			try withUnsafeMutablePointer(to: &outputData) { outputPointer in
				var translation = AudioValueTranslation(mInputData: inputPointer, mInputDataSize: UInt32(MemoryLayout<UInt32>.stride), mOutputData: outputPointer, mOutputDataSize: UInt32(MemoryLayout<CFString>.stride))
				try readAudioObjectProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyHighPassFilterSettingNameForIDCFString), scope: scope), from: objectID, into: &translation)
			}
		}
		return outputData as String
	}

	// High-pass filter setting helpers

	/// Returns the available high-pass filter settings
	/// - remark: This corresponds to the property `kAudioDevicePropertyHighPassFilterSettings`
	public func availableHighPassFilterSettings(inScope scope: PropertyScope) throws -> [HighPassFilterSetting] {
		return try highPassFilterSettings(inScope: scope).map { HighPassFilterSetting(device: self, scope: scope, id: $0) }
	}

	/// Returns the selected high-pass filter settings
	/// - remark: This corresponds to the property `kAudioDevicePropertyHighPassFilterSetting`
	public func selectedHighPassFilterSettings(inScope scope: PropertyScope) throws -> [HighPassFilterSetting] {
		return try highPassFilterSetting(inScope: scope).map { HighPassFilterSetting(device: self, scope: scope, id: $0) }
	}

	/// Returns the LFE volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeScalar`
	public func subVolumeScalar(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubVolumeScalar), scope: scope, element: channel))
	}
	/// Sets the LFE volume scalar for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeScalar`
	public func setSubVolumeScalar(_ value: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubVolumeScalar), scope: scope, element: channel), to: value)
	}

	/// Returns the LFE volume in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeDecibels`
	public func subVolumeDecibels(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubVolumeDecibels), scope: scope, element: channel))
	}
	/// Sets the LFE volume in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeDecibels`
	public func setSubVolumeDecibels(_ value: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws {
		return try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubVolumeDecibels), scope: scope, element: channel), to: value)
	}

	/// Returns the LFE volume range in decibels for `channel`
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeRangeDecibels`
	public func subVolumeRangeDecibels(forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> ClosedRange<Float> {
		let value: AudioValueRange = try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubVolumeRangeDecibels), scope: scope, element: channel))
		return Float(value.mMinimum) ... Float(value.mMaximum)
	}

	/// Converts LFE volume `scalar` to decibels and returns the converted value
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeScalarToDecibels`
	/// - parameter scalar: The value to convert
	public func convertSubVolumeToDecibels(fromScalar scalar: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubVolumeScalarToDecibels), scope: scope, element: channel), initialValue: scalar)
	}

	/// Converts LFE volume `decibels` to scalar and returns the converted value
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeDecibelsToScalar`
	/// - parameter decibels: The value to convert
	public func convertSubVolumeToScalar(fromDecibels decibels: Float, forChannel channel: PropertyElement = .master, inScope scope: PropertyScope = .global) throws -> Float {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubVolumeDecibelsToScalar), scope: scope, element: channel), initialValue: decibels)
	}

	/// Returns `true` if LFE are muted on `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertySubMute`
	public func subMute(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubMute), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether LFE are muted on `element`
	/// - remark: This corresponds to the property `kAudioDevicePropertySubMute`
	public func setSubMute(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertySubMute), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}
}

extension AudioDevice {
	/// Returns the volume decibels to scalar transfer function
	/// - remark: This corresponds to the property `kAudioDevicePropertyVolumeDecibelsToScalarTransferFunction`
	public func volumeDecibelsToScalarTransferFunction() throws -> AudioLevelControlTransferFunction {
		return AudioLevelControlTransferFunction(rawValue: try getProperty(PropertyAddress(kAudioDevicePropertyVolumeDecibelsToScalarTransferFunction)))!
	}
	/// Returns the play-through decibels to scalar transfer function
	/// - remark: This corresponds to the property `kAudioDevicePropertyPlayThruVolumeDecibelsToScalarTransferFunction`
	public func playThroughDecibelsToScalarTransferFunction() throws -> AudioLevelControlTransferFunction {
		return AudioLevelControlTransferFunction(rawValue: try getProperty(PropertyAddress(kAudioDevicePropertyPlayThruVolumeDecibelsToScalarTransferFunction)))!
	}

	/// Returns `true` if the device claims ownership of an attached iSub
	/// - remark: This corresponds to the property `kAudioDevicePropertyDriverShouldOwniSub`
	public func shouldOwniSub(inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws -> Bool {
		return try getProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDriverShouldOwniSub), scope: scope, element: element)) as UInt32 != 0
	}
	/// Sets whether the device should claim ownership of an attached iSub
	/// - remark: This corresponds to the property `kAudioDevicePropertyDriverShouldOwniSub`
	public func setShouldOwniSub(_ value: Bool, inScope scope: PropertyScope, onElement element: PropertyElement = .master) throws {
		try setProperty(PropertyAddress(PropertySelector(kAudioDevicePropertyDriverShouldOwniSub), scope: scope, element: element), to: UInt32(value ? 1 : 0))
	}

	/// Returns the LFE decibels to scalar transfer function
	/// - remark: This corresponds to the property `kAudioDevicePropertySubVolumeDecibelsToScalarTransferFunction`
	public func subDecibelsToScalarTransferFunction() throws -> AudioLevelControlTransferFunction {
		return AudioLevelControlTransferFunction(rawValue: try getProperty(PropertyAddress(kAudioDevicePropertySubVolumeDecibelsToScalarTransferFunction)))!
	}
}

extension AudioDevice {
	/// A thin wrapper around a HAL audio device transport type
	public struct TransportType: RawRepresentable, ExpressibleByIntegerLiteral, ExpressibleByStringLiteral {
		/// Unknown
		public static let unknown 			= TransportType(rawValue: kAudioDeviceTransportTypeUnknown)
		/// Built-in
		public static let builtIn 			= TransportType(rawValue: kAudioDeviceTransportTypeBuiltIn)
		/// Aggregate device
		public static let aggregate 		= TransportType(rawValue: kAudioDeviceTransportTypeAggregate)
		/// Virtual device
		public static let virtual 			= TransportType(rawValue: kAudioDeviceTransportTypeVirtual)
		/// PCI
		public static let pci 				= TransportType(rawValue: kAudioDeviceTransportTypePCI)
		/// USB
		public static let usb 				= TransportType(rawValue: kAudioDeviceTransportTypeUSB)
		/// FireWire
		public static let fireWire 			= TransportType(rawValue: kAudioDeviceTransportTypeFireWire)
		/// Bluetooth
		public static let bluetooth 		= TransportType(rawValue: kAudioDeviceTransportTypeBluetooth)
		/// Bluetooth Low Energy
		public static let bluetoothLE 		= TransportType(rawValue: kAudioDeviceTransportTypeBluetoothLE)
		/// HDMI
		public static let hdmi 				= TransportType(rawValue: kAudioDeviceTransportTypeHDMI)
		/// DisplayPort
		public static let displayPort 		= TransportType(rawValue: kAudioDeviceTransportTypeDisplayPort)
		/// AirPlay
		public static let airPlay 			= TransportType(rawValue: kAudioDeviceTransportTypeAirPlay)
		/// AVB
		public static let avb 				= TransportType(rawValue: kAudioDeviceTransportTypeAVB)
		/// Thunderbolt
		public static let thunderbolt 		= TransportType(rawValue: kAudioDeviceTransportTypeThunderbolt)
		/// Automatically-generated aggregate
		public static let autoAggregate 	= TransportType(rawValue: kAudioDeviceTransportTypeAutoAggregate)

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
		case kAudioDeviceTransportTypeUnknown:			return "Unknown"
		case kAudioDeviceTransportTypeBuiltIn:			return "Built-in"
		case kAudioDeviceTransportTypeAggregate: 		return "Aggregate"
		case kAudioDeviceTransportTypeVirtual:			return "Virtual"
		case kAudioDeviceTransportTypePCI:				return "PCI"
		case kAudioDeviceTransportTypeUSB:				return "USB"
		case kAudioDeviceTransportTypeFireWire:			return "FireWire"
		case kAudioDeviceTransportTypeBluetooth:		return "Bluetooth"
		case kAudioDeviceTransportTypeBluetoothLE: 		return "Bluetooth Low Energy"
		case kAudioDeviceTransportTypeHDMI:				return "HDMI"
		case kAudioDeviceTransportTypeDisplayPort:		return "DisplayPort"
		case kAudioDeviceTransportTypeAirPlay:			return "AirPlay"
		case kAudioDeviceTransportTypeAVB:				return "AVB"
		case kAudioDeviceTransportTypeThunderbolt: 		return "Thunderbolt"
		case kAudioDeviceTransportTypeAutoAggregate: 	return "Automatic Aggregate"
		default:										return "\(self.rawValue)"
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

extension AudioDevice {
	/// A play-through destination for an audio device
	public struct PlayThroughDestination {
		/// Returns the owning audio device
		public let device: AudioDevice
		/// Returns the play-through destination ID
		public let id: UInt32

		/// Returns the play-through destination name
		public func name() throws -> String {
			return try device.nameOfPlayThroughDestination(id)
		}
	}
}

extension AudioDevice.PlayThroughDestination: CustomDebugStringConvertible {
	public var debugDescription: String {
		if let name = try? name() {
			return "<\(type(of: self)): '\(id.fourCC)' \"\(name)\" on AudioDevice 0x\(String(device.objectID, radix: 16, uppercase: false))>"
		}
		else {
			return "<\(type(of: self)): '\(id.fourCC)' on AudioDevice 0x\(String(device.objectID, radix: 16, uppercase: false)))>"
		}
	}
}

extension AudioDevice {
	/// A channel nominal line level for an audio device
	public struct ChannelNominalLineLevel {
		/// Returns the owning audio device
		public let device: AudioDevice
		/// Returns the channel nominal line level scope
		public let scope: PropertyScope
		/// Returns the channel nominal line level ID
		public let id: UInt32

		/// Returns the channel nominal line level name
		public func name() throws -> String {
			return try device.nameOfChannelNominalLineLevel(id, inScope: scope)
		}
	}
}

extension AudioDevice.ChannelNominalLineLevel: CustomDebugStringConvertible {
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
	/// A high-pass filter setting for an audio device
	public struct HighPassFilterSetting {
		/// Returns the owning audio device
		public let device: AudioDevice
		/// Returns the high-pass filter setting scope
		public let scope: PropertyScope
		/// Returns the high-pass filter setting ID
		public let id: UInt32

		/// Returns the high-pass filter setting name
		public func name() throws -> String {
			return try device.nameOfHighPassFilterSetting(id, inScope: scope)
		}
	}
}

extension AudioDevice.HighPassFilterSetting: CustomDebugStringConvertible {
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
	/// Returns `true` if `self` has `selector` in `scope` on `element`
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public func hasSelector(_ selector: Selector<AudioDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Returns `true` if `selector` in `scope` on `element` is settable
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<AudioDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Registers `block` to be performed when `selector` in `scope` on `element` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<AudioDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element), perform: block)
	}
}

extension Selector where T == AudioDevice {
	/// The property selector `kAudioDevicePropertyConfigurationApplication`
	public static let configurationApplication = Selector(kAudioDevicePropertyConfigurationApplication)
	/// The property selector `kAudioDevicePropertyDeviceUID`
	public static let deviceUID = Selector(kAudioDevicePropertyDeviceUID)
	/// The property selector `kAudioDevicePropertyModelUID`
	public static let modelUID = Selector(kAudioDevicePropertyModelUID)
	/// The property selector `kAudioDevicePropertyTransportType`
	public static let transportType = Selector(kAudioDevicePropertyTransportType)
	/// The property selector `kAudioDevicePropertyRelatedDevices`
	/// The property selector `kAudioDevicePropertyRelatedDevices`
	public static let relatedDevices = Selector(kAudioDevicePropertyRelatedDevices)
	/// The property selector `kAudioDevicePropertyClockDomain`
	/// The property selector `kAudioDevicePropertyClockDomain`
	public static let clockDomain = Selector(kAudioDevicePropertyClockDomain)
	/// The property selector `kAudioDevicePropertyDeviceIsAlive`
	/// The property selector `kAudioDevicePropertyDeviceIsAlive`
	public static let deviceIsAlive = Selector(kAudioDevicePropertyDeviceIsAlive)
	/// The property selector `kAudioDevicePropertyDeviceIsRunning`
	/// The property selector `kAudioDevicePropertyDeviceIsRunning`
	public static let deviceIsRunning = Selector(kAudioDevicePropertyDeviceIsRunning)
	/// The property selector `kAudioDevicePropertyDeviceCanBeDefaultDevice`
	public static let deviceCanBeDefaultDevice = Selector(kAudioDevicePropertyDeviceCanBeDefaultDevice)
	/// The property selector `kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`
	public static let deviceCanBeDefaultSystemDevice = Selector(kAudioDevicePropertyDeviceCanBeDefaultSystemDevice)
	/// The property selector `kAudioDevicePropertyLatency`
	public static let latency = Selector(kAudioDevicePropertyLatency)
	/// The property selector `kAudioDevicePropertyStreams`
	public static let streams = Selector(kAudioDevicePropertyStreams)
	/// The property selector `kAudioObjectPropertyControlList`
	public static let controlList = Selector(kAudioObjectPropertyControlList)
	/// The property selector `kAudioDevicePropertySafetyOffset`
	public static let safetyOffset = Selector(kAudioDevicePropertySafetyOffset)
	/// The property selector `kAudioDevicePropertyNominalSampleRate`
	public static let nominalSampleRate = Selector(kAudioDevicePropertyNominalSampleRate)
	/// The property selector `kAudioDevicePropertyAvailableNominalSampleRates`
	public static let availableNominalSampleRates = Selector(kAudioDevicePropertyAvailableNominalSampleRates)
	/// The property selector `kAudioDevicePropertyIcon`
	public static let icon = Selector(kAudioDevicePropertyIcon)
	/// The property selector `kAudioDevicePropertyIsHidden`
	public static let isHidden = Selector(kAudioDevicePropertyIsHidden)
	/// The property selector `kAudioDevicePropertyPreferredChannelsForStereo`
	public static let preferredChannelsForStereo = Selector(kAudioDevicePropertyPreferredChannelsForStereo)
	/// The property selector `kAudioDevicePropertyPreferredChannelLayout`
	public static let preferredChannelLayout = Selector(kAudioDevicePropertyPreferredChannelLayout)

	/// The property selector `kAudioDevicePropertyPlugIn`
	public static let plugIn = Selector(kAudioDevicePropertyPlugIn)
	/// The property selector `kAudioDevicePropertyDeviceHasChanged`
	public static let hasChanged = Selector(kAudioDevicePropertyDeviceHasChanged)
	/// The property selector `kAudioDevicePropertyDeviceIsRunningSomewhere`
	public static let isRunningSomewhere = Selector(kAudioDevicePropertyDeviceIsRunningSomewhere)
	/// The property selector `kAudioDeviceProcessorOverload`
	public static let processorOverload = Selector(kAudioDeviceProcessorOverload)
	/// The property selector `kAudioDevicePropertyIOStoppedAbnormally`
	public static let ioStoppedAbornormally = Selector(kAudioDevicePropertyIOStoppedAbnormally)
	/// The property selector `kAudioDevicePropertyHogMode`
	public static let hogMode = Selector(kAudioDevicePropertyHogMode)
	/// The property selector `kAudioDevicePropertyBufferFrameSize`
	public static let bufferFrameSize = Selector(kAudioDevicePropertyBufferFrameSize)
	/// The property selector `kAudioDevicePropertyBufferFrameSizeRange`
	public static let bufferFrameSizeRange = Selector(kAudioDevicePropertyBufferFrameSizeRange)
	/// The property selector `kAudioDevicePropertyUsesVariableBufferFrameSizes`
	public static let usesVariableBufferFrameSizes = Selector(kAudioDevicePropertyUsesVariableBufferFrameSizes)
	/// The property selector `kAudioDevicePropertyIOCycleUsage`
	public static let ioCycleUsage = Selector(kAudioDevicePropertyIOCycleUsage)
	/// The property selector `kAudioDevicePropertyStreamConfiguration`
	public static let streamConfiguration = Selector(kAudioDevicePropertyStreamConfiguration)
	/// The property selector `kAudioDevicePropertyIOProcStreamUsage`
	public static let ioProcStreamUsage = Selector(kAudioDevicePropertyIOProcStreamUsage)
	/// The property selector `kAudioDevicePropertyActualSampleRate`
	public static let actualSampleRate = Selector(kAudioDevicePropertyActualSampleRate)
	/// The property selector `kAudioDevicePropertyClockDevice`
	public static let clockDevice = Selector(kAudioDevicePropertyClockDevice)
	/// The property selector `kAudioDevicePropertyIOThreadOSWorkgroup`
	public static let ioThreadOSWorkgroup = Selector(kAudioDevicePropertyIOThreadOSWorkgroup)

	/// The property selector `kAudioDevicePropertyJackIsConnected`
	public static let jackIsConnected = Selector(kAudioDevicePropertyJackIsConnected)
	/// The property selector `kAudioDevicePropertyVolumeScalar`
	public static let volumeScalar = Selector(kAudioDevicePropertyVolumeScalar)
	/// The property selector `kAudioDevicePropertyVolumeDecibels`
	public static let volumeDecibels = Selector(kAudioDevicePropertyVolumeDecibels)
	/// The property selector `kAudioDevicePropertyVolumeRangeDecibels`
	public static let volumeRangeDecibels = Selector(kAudioDevicePropertyVolumeRangeDecibels)
	/// The property selector `kAudioDevicePropertyVolumeScalarToDecibels`
	public static let volumeScalarToDecibels = Selector(kAudioDevicePropertyVolumeScalarToDecibels)
	/// The property selector `kAudioDevicePropertyVolumeDecibelsToScalar`
	public static let volumeDecibelsToScalar = Selector(kAudioDevicePropertyVolumeDecibelsToScalar)
	/// The property selector `kAudioDevicePropertyStereoPan`
	public static let stereoPan = Selector(kAudioDevicePropertyStereoPan)
	/// The property selector `kAudioDevicePropertyStereoPanChannels`
	public static let stereoPanChannels = Selector(kAudioDevicePropertyStereoPanChannels)
	/// The property selector `kAudioDevicePropertyMute`
	public static let mute = Selector(kAudioDevicePropertyMute)
	/// The property selector `kAudioDevicePropertySolo`
	public static let solo = Selector(kAudioDevicePropertySolo)
	/// The property selector `kAudioDevicePropertyPhantomPower`
	public static let phantomPower = Selector(kAudioDevicePropertyPhantomPower)
	/// The property selector `kAudioDevicePropertyPhaseInvert`
	public static let phaseInvert = Selector(kAudioDevicePropertyPhaseInvert)
	/// The property selector `kAudioDevicePropertyClipLight`
	public static let clipLight = Selector(kAudioDevicePropertyClipLight)
	/// The property selector `kAudioDevicePropertyTalkback`
	public static let talkback = Selector(kAudioDevicePropertyTalkback)
	/// The property selector `kAudioDevicePropertyListenback`
	public static let listenback = Selector(kAudioDevicePropertyListenback)
	/// The property selector `kAudioDevicePropertyDataSource`
	public static let dataSource = Selector(kAudioDevicePropertyDataSource)
	/// The property selector `kAudioDevicePropertyDataSources`
	public static let dataSources = Selector(kAudioDevicePropertyDataSources)
	/// The property selector `kAudioDevicePropertyDataSourceNameForIDCFString`
	public static let dataSourceNameForID = Selector(kAudioDevicePropertyDataSourceNameForIDCFString)
	/// The property selector `kAudioDevicePropertyDataSourceKindForID`
	public static let dataSourceKindForID = Selector(kAudioDevicePropertyDataSourceKindForID)
	/// The property selector `kAudioDevicePropertyClockSource`
	public static let clockSource = Selector(kAudioDevicePropertyClockSource)
	/// The property selector `kAudioDevicePropertyClockSources`
	public static let clockSources = Selector(kAudioDevicePropertyClockSources)
	/// The property selector `kAudioDevicePropertyClockSourceNameForIDCFString`
	public static let clockSourceNameForID = Selector(kAudioDevicePropertyClockSourceNameForIDCFString)
	/// The property selector `kAudioDevicePropertyClockSourceKindForID`
	public static let clockSourceKindForID = Selector(kAudioDevicePropertyClockSourceKindForID)
	/// The property selector `kAudioDevicePropertyPlayThru`
	public static let playThru = Selector(kAudioDevicePropertyPlayThru)
	/// The property selector `kAudioDevicePropertyPlayThruSolo`
	public static let playThruSolo = Selector(kAudioDevicePropertyPlayThruSolo)
	/// The property selector `kAudioDevicePropertyPlayThruVolumeScalar`
	public static let playThruVolumeScalar = Selector(kAudioDevicePropertyPlayThruVolumeScalar)
	/// The property selector `kAudioDevicePropertyPlayThruVolumeDecibels`
	public static let playThruVolumeDecibels = Selector(kAudioDevicePropertyPlayThruVolumeDecibels)
	/// The property selector `kAudioDevicePropertyPlayThruVolumeRangeDecibels`
	public static let playThruVolumeRangeDecibels = Selector(kAudioDevicePropertyPlayThruVolumeRangeDecibels)
	/// The property selector `kAudioDevicePropertyPlayThruVolumeScalarToDecibels`
	public static let playThruVolumeScalarToDecibels = Selector(kAudioDevicePropertyPlayThruVolumeScalarToDecibels)
	/// The property selector `kAudioDevicePropertyPlayThruVolumeDecibelsToScalar`
	public static let playThruVolumeDecibelsToScalar = Selector(kAudioDevicePropertyPlayThruVolumeDecibelsToScalar)
	/// The property selector `kAudioDevicePropertyPlayThruStereoPan`
	public static let playThruStereoPan = Selector(kAudioDevicePropertyPlayThruStereoPan)
	/// The property selector `kAudioDevicePropertyPlayThruStereoPanChannels`
	public static let playThruStereoPanChannels = Selector(kAudioDevicePropertyPlayThruStereoPanChannels)
	/// The property selector `kAudioDevicePropertyPlayThruDestination`
	public static let playThruDestination = Selector(kAudioDevicePropertyPlayThruDestination)
	/// The property selector `kAudioDevicePropertyPlayThruDestinations`
	public static let playThruDestinations = Selector(kAudioDevicePropertyPlayThruDestinations)
	/// The property selector `kAudioDevicePropertyPlayThruDestinationNameForIDCFString`
	public static let playThruDestinationNameForID = Selector(kAudioDevicePropertyPlayThruDestinationNameForIDCFString)
	/// The property selector `kAudioDevicePropertyChannelNominalLineLevel`
	public static let channelNominalLineLevel = Selector(kAudioDevicePropertyChannelNominalLineLevel)
	/// The property selector `kAudioDevicePropertyChannelNominalLineLevels`
	public static let channelNominalLineLevels = Selector(kAudioDevicePropertyChannelNominalLineLevels)
	/// The property selector `kAudioDevicePropertyChannelNominalLineLevelNameForIDCFString`
	public static let channelNominalLineLevelNameForID = Selector(kAudioDevicePropertyChannelNominalLineLevelNameForIDCFString)
	/// The property selector `kAudioDevicePropertyHighPassFilterSetting`
	public static let highPassFilterSetting = Selector(kAudioDevicePropertyHighPassFilterSetting)
	/// The property selector `kAudioDevicePropertyHighPassFilterSettings`
	public static let highPassFilterSettings = Selector(kAudioDevicePropertyHighPassFilterSettings)
	/// The property selector `kAudioDevicePropertyHighPassFilterSettingNameForIDCFString`
	public static let highPassFilterSettingNameForID = Selector(kAudioDevicePropertyHighPassFilterSettingNameForIDCFString)
	/// The property selector `kAudioDevicePropertySubVolumeScalar`
	public static let subVolumeScalar = Selector(kAudioDevicePropertySubVolumeScalar)
	/// The property selector `kAudioDevicePropertySubVolumeDecibels`
	public static let subVolumeDecibels = Selector(kAudioDevicePropertySubVolumeDecibels)
	/// The property selector `kAudioDevicePropertySubVolumeRangeDecibels`
	public static let subVolumeRangeDecibels = Selector(kAudioDevicePropertySubVolumeRangeDecibels)
	/// The property selector `kAudioDevicePropertySubVolumeScalarToDecibels`
	public static let subVolumeScalarToDecibels = Selector(kAudioDevicePropertySubVolumeScalarToDecibels)
	/// The property selector `kAudioDevicePropertySubVolumeDecibelsToScalar`
	public static let subVolumeDecibelsToScalar = Selector(kAudioDevicePropertySubVolumeDecibelsToScalar)
	/// The property selector `kAudioDevicePropertySubMute`
	public static let subMute = Selector(kAudioDevicePropertySubMute)

	/// The property selector `kAudioDevicePropertyVolumeDecibelsToScalarTransferFunction`
	public static let volumeDecibelsToScalarTransferFunction = Selector(kAudioDevicePropertyVolumeDecibelsToScalarTransferFunction)
	/// The property selector `kAudioDevicePropertyPlayThruVolumeDecibelsToScalarTransferFunction`
	public static let playThruVolumeDecibelsToScalarTransferFunction = Selector(kAudioDevicePropertyPlayThruVolumeDecibelsToScalarTransferFunction)
	/// The property selector `kAudioDevicePropertyDriverShouldOwniSub`
	public static let driverShouldOwniSub = Selector(kAudioDevicePropertyDriverShouldOwniSub)
	/// The property selector `kAudioDevicePropertySubVolumeDecibelsToScalarTransferFunction`
	public static let subVolumeDecibelsToScalarTransferFunction = Selector(kAudioDevicePropertySubVolumeDecibelsToScalarTransferFunction)
}
