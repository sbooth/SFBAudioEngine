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
		return try AudioSystemObject.instance.getProperty(PropertyAddress(kAudioHardwarePropertyTransportManagerList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioTransportManager }
	}

	/// Returns an initialized `AudioTransportManager` with `bundleID` or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioHardwarePropertyTranslateBundleIDToTransportManager` on `kAudioObjectSystemObject`
	/// - parameter bundleID: The desired bundle ID
	public class func makeTransportManager(forBundleID bundleID: String) throws -> AudioTransportManager? {
		guard let objectID = try AudioSystemObject.instance.transportManagerID(forBundleID: bundleID) else {
			return nil
		}
		return (AudioObject.make(objectID) as! AudioTransportManager)
	}

	public override var debugDescription: String {
		do {
			return "<\(type(of: self)): 0x\(String(objectID, radix: 16, uppercase: false)), [\(try endpointList().map({ $0.debugDescription }).joined(separator: ", "))]>"
		}
		catch {
			return super.debugDescription
		}
	}
}

extension AudioTransportManager {
	/// Creates and returns a new endpoint device
	/// - remark: This corresponds to the property `kAudioTransportManagerCreateEndPointDevice`
	/// - parameter composition: The composition of the new endpoint device
	/// - note: The constants for `composition` are defined in `AudioHardware.h`
	func createEndpointDevice(composition: [AnyHashable: Any]) throws -> AudioEndpointDevice {
		var qualifier = composition as CFDictionary
		return AudioObject.make(try getProperty(PropertyAddress(kAudioTransportManagerCreateEndPointDevice), type: AudioObjectID.self, qualifier: PropertyQualifier(&qualifier))) as! AudioEndpointDevice
	}

	/// Destroys an endpoint device
	/// - remark: This corresponds to the property `kAudioTransportManagerDestroyEndPointDevice`
	func destroyEndpointDevice(_ endpointDevice: AudioEndpointDevice) throws {
		_ = try getProperty(PropertyAddress(kAudioTransportManagerDestroyEndPointDevice), type: UInt32.self, initialValue: endpointDevice.objectID)
	}

	/// Returns the audio endpoints provided by the transport manager
	/// - remark: This corresponds to the property `kAudioTransportManagerPropertyEndPointList`
	public func endpointList() throws -> [AudioEndpoint] {
		return try getProperty(PropertyAddress(kAudioTransportManagerPropertyEndPointList), elementType: AudioObjectID.self).map { AudioObject.make($0) as! AudioEndpoint }
	}

	/// Returns the audio endpoint provided by the transport manager with the specified UID or `nil` if unknown
	/// - remark: This corresponds to the property `kAudioTransportManagerPropertyTranslateUIDToEndPoint`
	/// - parameter uid: The desired endpoint UID
	public func endpoint(forUID uid: String) throws -> AudioEndpoint? {
		var qualifierData = uid as CFString
		let endpointObjectID = try getProperty(PropertyAddress(kAudioTransportManagerPropertyTranslateUIDToEndPoint), type: AudioObjectID.self, qualifier: PropertyQualifier(&qualifierData))
		guard endpointObjectID != kAudioObjectUnknown else {
			return nil
		}
		return (AudioObject.make(endpointObjectID) as! AudioEndpoint)
	}

	/// Returns the transport type
	/// - remark: This corresponds to the property `kAudioTransportManagerPropertyTransportType`
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(PropertyAddress(kAudioTransportManagerPropertyTransportType), type: UInt32.self))
	}
}

extension AudioTransportManager {
	/// Returns `true` if `self` has `selector`
	/// - parameter selector: The selector of the desired property
	public func hasSelector(_ selector: AudioObjectSelector<AudioTransportManager>) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Returns `true` if `selector` is settable
	/// - parameter selector: The selector of the desired property
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: AudioObjectSelector<AudioTransportManager>) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue)))
	}

	/// Registers `block` to be performed when `selector` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: AudioObjectSelector<AudioTransportManager>, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue)), perform: block)
	}
}

extension AudioObjectSelector where T == AudioTransportManager {
	/// The property selector `kAudioTransportManagerCreateEndPointDevice`
//	public static let createEndpointDevice = Selector(kAudioTransportManagerCreateEndPointDevice)
	/// The property selector `kAudioTransportManagerDestroyEndPointDevice`
//	public static let destroyEndpointDevice = Selector(kAudioTransportManagerDestroyEndPointDevice)
	/// The property selector `kAudioTransportManagerPropertyEndPointList`
	public static let endpointList = AudioObjectSelector(kAudioTransportManagerPropertyEndPointList)
	/// The property selector `kAudioTransportManagerPropertyTranslateUIDToEndPoint`
	public static let translateUIDToEndpoint = AudioObjectSelector(kAudioTransportManagerPropertyTranslateUIDToEndPoint)
	/// The property selector `kAudioTransportManagerPropertyTransportType`
	public static let transportType = AudioObjectSelector(kAudioTransportManagerPropertyTransportType)
}
