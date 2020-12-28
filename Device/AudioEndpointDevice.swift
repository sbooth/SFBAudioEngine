//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A HAL audio endpoint device (`kAudioEndPointDeviceClassID`)
public class AudioEndpointDevice: AudioDevice {
}

extension AudioEndpointDevice {
	/// Returns the composition (`kAudioEndPointDevicePropertyComposition`)
	public func composition() throws -> [AnyHashable: Any] {
		return try getProperty(AudioObjectProperty(kAudioEndPointDevicePropertyComposition))
	}

	/// Returns the audio endpoints owned by `self` (`kAudioEndPointDevicePropertyEndPointList`)
	public func endpointList() throws -> [AudioEndpoint] {
		return try getProperty(AudioObjectProperty(kAudioEndPointDevicePropertyEndPointList)).map { AudioObject.make($0) as! AudioEndpoint }
	}

	/// Returns the owning `pid_t`or `0` for public devices (`kAudioEndPointDevicePropertyIsPrivate`)
	public func isPrivate() throws -> pid_t {
		return try getProperty(AudioObjectProperty(kAudioEndPointDevicePropertyIsPrivate))
	}
}
