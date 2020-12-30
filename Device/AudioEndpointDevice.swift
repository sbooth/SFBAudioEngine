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
