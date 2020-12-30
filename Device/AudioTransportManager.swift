//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio transport manager object
///
/// This class has a single scope (`kAudioObjectPropertyScopeGlobal`) and a single element (`kAudioObjectPropertyElementMaster`)
/// - remark: This class correponds to objects with base class `kAudioTransportManagerClassID`
public class AudioTransportManager: AudioPlugIn {
	/// Returns the available audio transport managers
	/// - remark: This corresponds to the property`kAudioHardwarePropertyTransportManagerList` on `kAudioObjectSystemObject`
	public class func transportManagers() throws -> [AudioTransportManager] {
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTransportManagerList)).map { AudioObject.make($0) as! AudioTransportManager }
	}

	/// Initializes an `AudioTransportManager` with `bundleID`
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateBundleIDToTransportManager` on `kAudioObjectSystemObject`
	/// - parameter bundleID: The desired bundle ID
	public convenience init?(_ bundleID: String) {
		var qualifier = bundleID as CFString
		guard let transportManagerObjectID: AudioObjectID = try? AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTranslateBundleIDToTransportManager), qualifier: PropertyQualifier(&qualifier)), transportManagerObjectID != kAudioObjectUnknown else {
			return nil
		}
		self.init(transportManagerObjectID)
	}
}

extension AudioTransportManager {
	/// Creates and returns a new endpoint device
	/// - remark: This corresponds to the property `kAudioTransportManagerCreateEndPointDevice`
	/// - parameter composition: The composition of the new endpoint device
	/// - note: The constants for `composition` are defined in `AudioHardware.h`
	func createEndpointDevice(composition: [AnyHashable: Any]) throws -> AudioDevice {
		var qualifier = composition as CFDictionary
		return AudioObject.make(try getProperty(PropertyAddress(kAudioTransportManagerCreateEndPointDevice), qualifier: PropertyQualifier(&qualifier))) as! AudioDevice
	}

	/// Destroys an endpoint device
	/// - remark: This corresponds to the property `kAudioTransportManagerDestroyEndPointDevice`
	func destroyEndpointDevice(_ endpointDevice: AudioDevice) throws {
		_ = try getProperty(PropertyAddress(kAudioTransportManagerDestroyEndPointDevice), initialValue: endpointDevice.objectID)
	}

	/// Returns the audio endpoints provided by the transport manager
	/// - remark: This corresponds to the property `kAudioTransportManagerPropertyEndPointList`
	public func endpointList() throws -> [AudioObject] {
		return try getProperty(PropertyAddress(kAudioTransportManagerPropertyEndPointList)).map { AudioObject.make($0) }
	}

	/// Returns the audio endpoint provided by the transport manager with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioTransportManagerPropertyTranslateUIDToEndPoint`
	/// - parameter uid: The desired endpoint UID
	public func endpoint(_ uid: String) throws -> AudioObject? {
		var qualifierData = uid as CFString
		let endpointObjectID: AudioObjectID = try getProperty(PropertyAddress(kAudioTransportManagerPropertyTranslateUIDToEndPoint), qualifier: PropertyQualifier(&qualifierData))
		guard endpointObjectID != kAudioObjectUnknown else {
			return nil
		}
		return AudioObject.make(endpointObjectID)
	}

	/// Returns the transport type
	/// - remark: This corresponds to the property `kAudioTransportManagerPropertyTransportType`
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(PropertyAddress(kAudioTransportManagerPropertyTransportType)))
	}
}
