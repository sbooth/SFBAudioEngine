/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioTransportManager {
	/// Returns the audio endpoints provided by the transport manager
	/// - note: This corresponds to `kAudioTransportManagerPropertyEndPointList`
	func endpoints() throws -> [AudioObject] {
		return try audioObjectsForProperty(.transportManagerEndPointList)
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioTransportManagerPropertyTransportType`
	func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: UInt32(try uintForProperty(.transportManagerTransportType)))!
	}
}