/*
* Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
* See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
*/

import Foundation

extension AudioTransportManager {
	/// Returns the audio endpoints provided by the transport manager
	/// - note: This corresponds to `kAudioTransportManagerPropertyEndPointList`
	/// - throws: An error if the property could not be retrieved
	public func endpoints() throws -> [AudioEndpoint] {
		return try getProperty(.transportManagerEndPointList) as [AudioObject] as! [AudioEndpoint]
	}

	/// Returns the transport type
	/// - note: This corresponds to `kAudioTransportManagerPropertyTransportType`
	/// - throws: An error if the property could not be retrieved
	public func transportType() throws -> AudioDevice.TransportType {
		return AudioDevice.TransportType(rawValue: try getProperty(.transportManagerTransportType))!
	}
}
