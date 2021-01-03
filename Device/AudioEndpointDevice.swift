//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio endpoint device
/// - remark: This class correponds to objects with base class `kAudioEndPointDeviceClassID`
public class AudioEndpointDevice: AudioDevice {
}

extension AudioEndpointDevice {
	/// Returns the composition
	/// - remark: This corresponds to the property `kAudioEndPointDevicePropertyComposition`
	public func composition() throws -> [AnyHashable: Any] {
		return try getProperty(PropertyAddress(kAudioEndPointDevicePropertyComposition))
	}

	/// Returns the audio endpoints owned by `self`
	/// - remark: This corresponds to the property `kAudioEndPointDevicePropertyEndPointList`
	public func endpointList() throws -> [AudioEndpoint] {
		return try getProperty(PropertyAddress(kAudioEndPointDevicePropertyEndPointList)).map { AudioObject.make($0) as! AudioEndpoint }
	}

	/// Returns the owning `pid_t`or `0` for public devices
	/// - remark: This corresponds to the property `kAudioEndPointDevicePropertyIsPrivate`
	public func isPrivate() throws -> pid_t {
		return try getProperty(PropertyAddress(kAudioEndPointDevicePropertyIsPrivate))
	}
}

extension AudioEndpointDevice {
	/// Returns `true` if `self` has `selector` in `scope` on `element`
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	public func hasSelector(_ selector: Selector<AudioEndpointDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) -> Bool {
		return hasProperty(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Returns `true` if `selector` in `scope` on `element` is settable
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - throws: An error if `self` does not have the requested property
	public func isSelectorSettable(_ selector: Selector<AudioEndpointDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master) throws -> Bool {
		return try isPropertySettable(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element))
	}

	/// Registers `block` to be performed when `selector` in `scope` on `element` changes
	/// - parameter selector: The selector of the desired property
	/// - parameter scope: The desired scope
	/// - parameter element: The desired element
	/// - parameter block: A closure to invoke when the property changes or `nil` to remove the previous value
	/// - throws: An error if the property listener could not be registered
	public func whenSelectorChanges(_ selector: Selector<AudioEndpointDevice>, inScope scope: PropertyScope = .global, onElement element: PropertyElement = .master, perform block: PropertyChangeNotificationBlock?) throws {
		try whenPropertyChanges(PropertyAddress(PropertySelector(selector.rawValue), scope: scope, element: element), perform: block)
	}
}

extension AudioObject.Selector where T == AudioEndpointDevice {
	/// The property selector `kAudioEndPointDevicePropertyComposition`
	public static let composition = AudioObject.Selector(kAudioEndPointDevicePropertyComposition)
	/// The property selector `kAudioEndPointDevicePropertyEndPointList`
	public static let endpointList = AudioObject.Selector(kAudioEndPointDevicePropertyEndPointList)
	/// The property selector `kAudioEndPointDevicePropertyIsPrivate`
	public static let isPrivate = AudioObject.Selector(kAudioEndPointDevicePropertyIsPrivate)
}
