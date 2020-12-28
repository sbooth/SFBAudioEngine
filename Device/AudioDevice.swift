//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio device object (`kAudioDeviceClassID`)
///
/// This class has four scopes (`kAudioObjectPropertyScopeGlobal`, `kAudioObjectPropertyScopeInput`, `kAudioObjectPropertyScopeOutput`, and `kAudioObjectPropertyScopePlayThrough`), a master element (`kAudioObjectPropertyElementMaster`), and an element for each channel in each stream
public class AudioDevice: AudioObject {
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

	/// Returns the available audio devices (`kAudioHardwarePropertyDevices` from `kAudioObjectSystemObject`)
	public class func devices() throws -> [AudioDevice] {
		try AudioSystemObject.instance.getProperty(AudioObjectProperty(kAudioHardwarePropertyDevices)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the default input device (`kAudioHardwarePropertyDefaultInputDevice` from `kAudioObjectSystemObject`)
	public class func defaultInputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(AudioObjectProperty(kAudioHardwarePropertyDefaultInputDevice))) as! AudioDevice
	}

	/// Returns the default output device (`kAudioHardwarePropertyDefaultOutputDevice` from `kAudioObjectSystemObject`)
	public class func defaultOutputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(AudioObjectProperty(kAudioHardwarePropertyDefaultOutputDevice))) as! AudioDevice
	}

	/// Returns the default system output device (`kAudioHardwarePropertyDefaultSystemOutputDevice` from `kAudioObjectSystemObject`)
	public class func defaultSystemOutputDevice() throws -> AudioDevice {
		return AudioObject.make(try AudioSystemObject.instance.getProperty(AudioObjectProperty(kAudioHardwarePropertyDefaultSystemOutputDevice))) as! AudioDevice
	}

	/// Initializes an `AudioDevice` with `uid`
	/// - parameter uid: The desired device UID
	public convenience init?(_ uid: String) {
		var qualifier = uid as CFString
		guard let deviceObjectID: AudioObjectID = try? AudioSystemObject.instance.getProperty(AudioObjectProperty(kAudioHardwarePropertyTranslateUIDToDevice), qualifier: PropertyQualifier(&qualifier)), deviceObjectID != kAudioObjectUnknown else {
			return nil
		}
		self.init(deviceObjectID)
	}
}

// MARK: - Audio Device Base Properties

extension AudioDevice {
	/// Returns the configuration application (`kAudioDevicePropertyConfigurationApplication`)
	public func configurationApplication() throws -> String {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyConfigurationApplication))
	}

	/// Returns the device UID (`kAudioDevicePropertyDeviceUID`)
	public func deviceUID() throws -> String {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyDeviceUID))
	}

	/// Returns the model UID (`kAudioDevicePropertyModelUID`)
	public func modelUID() throws -> String {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyModelUID))
	}

	/// Returns the transport type (`kAudioDevicePropertyTransportType`)
	public func transportType() throws -> TransportType {
		return TransportType(rawValue: try getProperty(AudioObjectProperty(kAudioDevicePropertyTransportType)))
	}

	/// Returns related audio devices (`kAudioDevicePropertyRelatedDevices`)
	public func relatedDevices() throws -> [AudioDevice] {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyRelatedDevices)).map { AudioObject.make($0) as! AudioDevice }
	}

	/// Returns the clock domain (`kAudioClockDevicePropertyClockDomain`)
	public func clockDomain() throws -> UInt32 {
		return try getProperty(AudioObjectProperty(kAudioClockDevicePropertyClockDomain))
	}

	/// Returns `true` if the device is alive (`kAudioDevicePropertyDeviceIsAlive`)
	public func isAlive() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyModelUID)) != 0
	}

	/// Returns `true` if the device is running (`kAudioDevicePropertyDeviceIsRunning`)
	public func isRunning() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyDeviceIsRunning)) != 0
	}
	/// Starts or stops the device (`kAudioDevicePropertyDeviceIsRunning`)
	/// - parameter value: The desired property value
	public func setIsRunning(_ value: Bool) throws {
		try setProperty(AudioObjectProperty(kAudioDevicePropertyDeviceIsRunning), to: value ? 1 : 0)
	}

	/// Returns `true` if the device can be the default device (`kAudioDevicePropertyDeviceCanBeDefaultDevice`)
	/// - parameter scope: The desired scope
	public func canBeDefault(in scope: PropertyScope) throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyDeviceCanBeDefaultDevice)) != 0
	}

	/// Returns `true` if the device can be the system default device (`kAudioDevicePropertyDeviceCanBeDefaultSystemDevice`)
	/// - parameter scope: The desired scope
	public func canBeSystemDefault(in scope: PropertyScope) throws -> Bool {
		return try getProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertyDeviceCanBeDefaultSystemDevice), in: scope)) != 0
	}

	/// Returns the latency (`kAudioDevicePropertyLatency`)
	/// - parameter scope: The desired scope
	public func latency(in scope: PropertyScope) throws -> UInt32 {
		return try getProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertyLatency), in: scope))
	}

	/// Returns the device's streams (`kAudioDevicePropertyStreams`)
	/// - parameter scope: The desired scope
	public func streams(in scope: PropertyScope) throws -> [AudioStream] {
		return try getProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertyStreams), in: scope)).map { AudioObject.make($0) as! AudioStream }
	}

	/// Returns the device's audio controls (`kAudioObjectPropertyControlList`)
	/// - parameter scope: The desired scope
	public func controlList(in scope: PropertyScope) throws -> [AudioControl] {
		return try getProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioObjectPropertyControlList), in: scope)).map { AudioObject.make($0) as! AudioControl }
	}

	/// Returns the safety offset (`kAudioDevicePropertySafetyOffset`)
	/// - parameter scope: The desired scope
	public func safetyOffset(in scope: PropertyScope) throws -> UInt32 {
		return try getProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertySafetyOffset), in: scope))
	}

	/// Returns the sample rate (`kAudioDevicePropertyNominalSampleRate`)
	public func sampleRate() throws -> Double {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyNominalSampleRate))
	}
	/// Sets the sample rate (`kAudioDevicePropertyNominalSampleRate`)
	/// - parameter value: The desired property value
	public func setSampleRate(_ value: Double) throws {
		try setProperty(AudioObjectProperty(kAudioDevicePropertyNominalSampleRate), to: value)
	}

	/// Returns the available sample rates (`kAudioDevicePropertyAvailableNominalSampleRates`)
	public func availableSampleRates() throws -> [AudioValueRange] {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyAvailableNominalSampleRates))
	}

	/// Returns the URL of the device's icon (`kAudioDevicePropertyIcon`)
	public func icon() throws -> URL {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyIcon))
	}

	/// Returns `true` if the device is hidden (`kAudioDevicePropertyIsHidden`)
	public func isHidden() throws -> Bool {
		return try getProperty(AudioObjectProperty(kAudioDevicePropertyIsHidden)) != 0
	}

	/// Returns the preferred stereo channels for the device (`kAudioDevicePropertyPreferredChannelsForStereo`)
	/// - parameter scope: The desired scope
	public func preferredStereoChannels(in scope: PropertyScope) throws -> (UInt32, UInt32) {
		let channels: [UInt32] = try getProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), in: scope))
		precondition(channels.count == 2)
		return (channels[0], channels[1])
	}

	/// Sets the preferred stereo channels (`kAudioDevicePropertyPreferredChannelsForStereo`)
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
	public func setPreferredStereoChannels(_ value: (UInt32, UInt32), scope: PropertyScope) throws {
		try setProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelsForStereo), in: scope), to: [value.0, value.1])
	}

	/// Returns the preferred channel layout (`kAudioDevicePropertyPreferredChannelLayout`)
	/// - parameter scope: The desired scope
	public func preferredChannelLayout(in scope: PropertyScope) throws -> AudioChannelLayoutWrapper {
		return try getAudioObjectProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelLayout), in: scope), from: objectID)
	}
	/// Sets the preferred channel layout (`kAudioDevicePropertyPreferredChannelLayout`)
	/// - parameter value: The desired property value
	/// - parameter scope: The desired scope
//	public func setPreferredChannelLayout(_ value: AudioChannelLayout, in scope: PropertyScope) throws {
//		try setProperty(AudioObjectProperty(PropertySelector(rawValue: kAudioDevicePropertyPreferredChannelLayout), in: scope), to: value)
//	}
}
