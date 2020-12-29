//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio
import os.log

extension Notification.Name {
	/// Posted when audio devices (`kAudioHardwarePropertyDevices` on `kAudioObjectSystemObject`) change
	public static let audioDevicesChanged = Notification.Name("org.sbooth.AudioEngine.AudioDevice.Notifier")
}

/// A class that broadcasts notifications when the available audio devices change
class AudioDeviceNotifier {
	/// The singleton audio device notifier
	public static var instance = AudioDeviceNotifier()

	/// `kAudioHardwarePropertyDevices`on `kAudioObjectSystemObject` listener block
	private let listenerBlock: AudioObjectPropertyListenerBlock

	/// Initializes an `AudioDeviceNotifier`
	private init() {
		listenerBlock = { inNumberAddresses, inAddresses in
//			let buf = UnsafeBufferPointer(start: inAddresses, count: Int(inNumberAddresses))
			NotificationCenter.default.post(name: .audioDevicesChanged, object: nil)
		}

		var propertyAddress = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDevices, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMaster)
		let result = AudioObjectAddPropertyListenerBlock(AudioObjectID(kAudioObjectSystemObject), &propertyAddress, DispatchQueue.global(qos: .default), listenerBlock)
		if result != kAudioHardwareNoError {
			os_log(.error, log: audioObjectLog, "AudioObjectAddPropertyListenerBlock (kAudioHardwarePropertyDevices) failed: '%{public}@'", UInt32(result).fourCC)
		}
	}

	deinit {
		var propertyAddress = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDevices, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMaster)
		let result = AudioObjectRemovePropertyListenerBlock(AudioObjectID(kAudioObjectSystemObject), &propertyAddress, DispatchQueue.global(qos: .default), listenerBlock)
		if result != kAudioHardwareNoError {
			os_log(.error, log: audioObjectLog, "AudioObjectRemovePropertyListenerBlock (kAudioHardwarePropertyDevices) failed: '%{public}@'", UInt32(result).fourCC)
		}
	}
}
