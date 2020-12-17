/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Foundation

extension AudioDevice {
	
	/// Returns an array of available sample rates or `[]` on error
	public var availableSampleRates: [Double] {
		guard let sampleRates = __availableSampleRates else {
			return []
		}
		return sampleRates.map { $0.doubleValue }
	}

	/// Returns the preferred stereo channels for the device
	/// - note: This is the property `{ kAudioDevicePropertyPreferredChannelsForStereo, scope, kAudioObjectPropertyElementMaster }`
	/// - parameter scope: The desired scope
	/// - returns: The preferred stereo channels for the device
	public func preferredStereoChannels(_ scope: PropertyScope) -> (left: UInt32, right: UInt32)? {
		guard let preferredChannels = self.__preferredStereoChannels(in: scope), preferredChannels.count == 2 else {
			return nil;
		}
		return (preferredChannels[0].uint32Value, preferredChannels[1].uint32Value)
	}

	public func foo()
	{
		for s in streams! {
//			print("\(s.physicalFormat)")
//			print("\(s.virtualFormat)")
		}

		var asbd = AudioStreamBasicDescription(mSampleRate: 44100, mFormatID: kAudioFormatLinearPCM, mFormatFlags: kAudioFormatFlagIsFloat|kAudioFormatFlagIsNonInterleaved, mBytesPerPacket: 32, mFramesPerPacket: 1, mBytesPerFrame: 32, mChannelsPerFrame: 4, mBitsPerChannel: 32, mReserved: 0)
		let f = AVAudioFormat(streamDescription: &asbd)
		print("\(f)")

		return

		whenPropertyChanges(.baseClass) {
			print("foo")
		}

		hasProperty(.baseClass, scope: .global, element: .master)
		let props: [AudioObjectPropertySelector] = [
			kAudioDevicePropertyConfigurationApplication,
			kAudioDevicePropertyDeviceUID,
			kAudioDevicePropertyModelUID,
			kAudioDevicePropertyTransportType,
			kAudioDevicePropertyRelatedDevices,
			kAudioDevicePropertyClockDomain,
			kAudioDevicePropertyDeviceIsAlive,
			kAudioDevicePropertyDeviceIsRunning,
			kAudioDevicePropertyDeviceCanBeDefaultDevice,
			kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
			kAudioDevicePropertyLatency,
			kAudioDevicePropertyStreams,
			kAudioObjectPropertyControlList,
			kAudioDevicePropertySafetyOffset,
			kAudioDevicePropertyNominalSampleRate,
			kAudioDevicePropertyAvailableNominalSampleRates,
			kAudioDevicePropertyIcon,
			kAudioDevicePropertyIsHidden,
			kAudioDevicePropertyPreferredChannelsForStereo,
			kAudioDevicePropertyPreferredChannelLayout
		]

		let scopes: [PropertyScope] = [.global, .input, .output, .playThrough]

		for prop in props {
			for scope in scopes {
				let p = PropertySelector(rawValue: prop)!
				let has = hasProperty(p, scope: scope)
//				print("hasProperty( '\(FourCC(prop))', \(scope) ) = \(has)")
				print("hasProperty( \(p), \(scope) ) = \(has)")
			}
		}
	}
}
