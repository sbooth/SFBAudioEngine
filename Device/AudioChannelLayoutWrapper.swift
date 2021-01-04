//
// Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

/// A thin wrapper around a variable-length `AudioChannelLayout` structure
public struct AudioChannelLayoutWrapper {
	/// The underlying memory
	let ptr: UnsafePointer<UInt8>

	/// Creates a new `AudioChannelLayoutWrapper` instance wrapping `mem`
	init(_ mem: UnsafePointer<UInt8>) {
		ptr = mem
	}

	/// Returns the layout's `mAudioChannelLayoutTag`
	public var tag: AudioChannelLayoutTag {
		return ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { $0.pointee.mChannelLayoutTag }
	}

	/// Returns the layout's `mAudioChannelBitmap`
	public var bitmap: AudioChannelBitmap {
		return ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { $0.pointee.mChannelBitmap }
	}

	/// Returns the layout's `mNumberChannelDescriptions`
	public var numberChannelDescriptions: UInt32 {
		return ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { $0.pointee.mNumberChannelDescriptions }
	}

	/// Returns the layout's `mChannelDescriptions`
	public var channelDescriptions: UnsafeBufferPointer<AudioChannelDescription> {
		let count = Int(numberChannelDescriptions)
		// Does not compile (!) : MemoryLayout<AudioChannelLayout>.offset(of: \.mChannelDescriptions)
		let offset = MemoryLayout.offset(of: \AudioChannelLayout.mChannelDescriptions)!
		let chanPtr = UnsafeRawPointer(ptr.advanced(by: offset)).assumingMemoryBound(to: AudioChannelDescription.self)
		return UnsafeBufferPointer<AudioChannelDescription>(start: chanPtr, count: count)
	}

	/// Performs `block` with a pointer to the underlying `AudioChannelLayout` structure
	public func withUnsafePointer<T>(_ block: (UnsafePointer<AudioChannelLayout>) throws -> T) rethrows -> T {
		return try ptr.withMemoryRebound(to: AudioChannelLayout.self, capacity: 1) { return try block($0) }
	}
}

import AVFoundation

extension AudioChannelLayoutWrapper {
	/// Returns `self` converted to an `AVAudioChannelLayout`object
	public var avAudioChannelLayout: AVAudioChannelLayout {
		return withUnsafePointer { return AVAudioChannelLayout(layout: $0) }
	}
}

extension AudioChannelLayoutWrapper: CustomDebugStringConvertible {
	public var debugDescription: String {
		let channelLayoutTag = tag
		if channelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions {
			return "<\(type(of: self)): mNumberChannelDescriptions = \(numberChannelDescriptions), mChannelDescriptions = [\(channelDescriptions.map({ $0.channelDescription }).joined(separator: ", "))]>"
		}
		else if channelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap {
			return "<\(type(of: self)): mChannelBitmap = \(bitmap.bitmapDescription)>"
		}

		return "<\(type(of: self)): mChannelLayoutTag = \(channelLayoutTag.channelLayoutTagName)"
	}
}

extension AudioChannelDescription {
	public var channelDescription: String {
		if mChannelLabel == kAudioChannelLabel_UseCoordinates {
			return "mChannelFlags = \(mChannelFlags.channelFlagsDescription), mCoordinates = (\(mCoordinates.0), \(mCoordinates.1), \(mCoordinates.2)"
		}
		else {
			return "mChannelLabel = \(mChannelLabel.channelLabelName)"
		}
	}
}

extension AudioChannelLayoutTag {
	/// Returns the name of the `AudioChannelLayoutTag`value of `self`
	var channelLayoutTagName: String {
		switch self {
		case kAudioChannelLayoutTag_Mono:					return "Mono"
		case kAudioChannelLayoutTag_Stereo:					return "Stereo"
		case kAudioChannelLayoutTag_StereoHeadphones:		return "Stereo Headphones"
		case kAudioChannelLayoutTag_MatrixStereo:			return "Matrix Stereo"
		case kAudioChannelLayoutTag_MidSide:				return "Mid-Side"
		case kAudioChannelLayoutTag_XY:						return "XY"
		case kAudioChannelLayoutTag_Binaural:				return "Binaural"
		case kAudioChannelLayoutTag_Ambisonic_B_Format:		return "Ambisonic_B_Format"
		case kAudioChannelLayoutTag_Quadraphonic:			return "Quadraphonic"
		case kAudioChannelLayoutTag_Pentagonal:				return "Pentagonal"
		case kAudioChannelLayoutTag_Hexagonal:				return "Hexagonal"
		case kAudioChannelLayoutTag_Octagonal:				return "Octagonal"
		case kAudioChannelLayoutTag_Cube:					return "Cube"
		case kAudioChannelLayoutTag_MPEG_3_0_A:				return "MPEG_3_0_A"
		case kAudioChannelLayoutTag_MPEG_3_0_B:				return "MPEG_3_0_B"
		case kAudioChannelLayoutTag_MPEG_4_0_A:				return "MPEG_4_0_A"
		case kAudioChannelLayoutTag_MPEG_4_0_B:				return "MPEG_4_0_B"
		case kAudioChannelLayoutTag_MPEG_5_0_A:				return "MPEG_5_0_A"
		case kAudioChannelLayoutTag_MPEG_5_0_B:				return "MPEG_5_0_B"
		case kAudioChannelLayoutTag_MPEG_5_0_C:				return "MPEG_5_0_C"
		case kAudioChannelLayoutTag_MPEG_5_0_D:				return "MPEG_5_0_D"
		case kAudioChannelLayoutTag_MPEG_5_1_A:				return "MPEG_5_1_A"
		case kAudioChannelLayoutTag_MPEG_5_1_B:				return "MPEG_5_1_B"
		case kAudioChannelLayoutTag_MPEG_5_1_C:				return "MPEG_5_1_C"
		case kAudioChannelLayoutTag_MPEG_5_1_D:				return "MPEG_5_1_D"
		case kAudioChannelLayoutTag_MPEG_6_1_A:				return "MPEG_6_1_A"
		case kAudioChannelLayoutTag_MPEG_7_1_A:				return "MPEG_7_1_A"
		case kAudioChannelLayoutTag_MPEG_7_1_B:				return "MPEG_7_1_B"
		case kAudioChannelLayoutTag_MPEG_7_1_C:				return "MPEG_7_1_C"
		case kAudioChannelLayoutTag_Emagic_Default_7_1:		return "Emagic_Default_7_1"
		case kAudioChannelLayoutTag_SMPTE_DTV:				return "SMPTE_DTV"
		case kAudioChannelLayoutTag_ITU_2_1:				return "ITU_2_1"
		case kAudioChannelLayoutTag_ITU_2_2:				return "ITU_2_2"
		case kAudioChannelLayoutTag_DVD_4:					return "DVD_4"
		case kAudioChannelLayoutTag_DVD_5:					return "DVD_5"
		case kAudioChannelLayoutTag_DVD_6:					return "DVD_6"
		case kAudioChannelLayoutTag_DVD_10:					return "DVD_10"
		case kAudioChannelLayoutTag_DVD_11:					return "DVD_11"
		case kAudioChannelLayoutTag_DVD_18:					return "DVD_18"
		case kAudioChannelLayoutTag_AudioUnit_6_0:			return "AudioUnit_6_0"
		case kAudioChannelLayoutTag_AudioUnit_7_0:			return "AudioUnit_7_0"
		case kAudioChannelLayoutTag_AudioUnit_7_0_Front:	return "AudioUnit_7_0_Front"
		case kAudioChannelLayoutTag_AAC_6_0:				return "AAC_6_0"
		case kAudioChannelLayoutTag_AAC_6_1:				return "AAC_6_1"
		case kAudioChannelLayoutTag_AAC_7_0:				return "AAC_7_0"
		case kAudioChannelLayoutTag_AAC_7_1_B:				return "AAC_7_1_B"
		case kAudioChannelLayoutTag_AAC_7_1_C:				return "AAC_7_1_C"
		case kAudioChannelLayoutTag_AAC_Octagonal:			return "AAC_Octagonal"
		case kAudioChannelLayoutTag_TMH_10_2_std:			return "TMH_10_2_std"
		case kAudioChannelLayoutTag_TMH_10_2_full:			return "TMH_10_2_full"
		case kAudioChannelLayoutTag_AC3_1_0_1:				return "AC3_1_0_1"
		case kAudioChannelLayoutTag_AC3_3_0:				return "AC3_3_0"
		case kAudioChannelLayoutTag_AC3_3_1:				return "AC3_3_1"
		case kAudioChannelLayoutTag_AC3_3_0_1:				return "AC3_3_0_1"
		case kAudioChannelLayoutTag_AC3_2_1_1:				return "AC3_2_1_1"
		case kAudioChannelLayoutTag_AC3_3_1_1:				return "AC3_3_1_1"
		case kAudioChannelLayoutTag_EAC_6_0_A:				return "EAC_6_0_A"
		case kAudioChannelLayoutTag_EAC_7_0_A:				return "EAC_7_0_A"
		case kAudioChannelLayoutTag_EAC3_6_1_A:				return "EAC3_6_1_A"
		case kAudioChannelLayoutTag_EAC3_6_1_B:				return "EAC3_6_1_B"
		case kAudioChannelLayoutTag_EAC3_6_1_C:				return "EAC3_6_1_C"
		case kAudioChannelLayoutTag_EAC3_7_1_A:				return "EAC3_7_1_A"
		case kAudioChannelLayoutTag_EAC3_7_1_B:				return "EAC3_7_1_B"
		case kAudioChannelLayoutTag_EAC3_7_1_C:				return "EAC3_7_1_C"
		case kAudioChannelLayoutTag_EAC3_7_1_D:				return "EAC3_7_1_D"
		case kAudioChannelLayoutTag_EAC3_7_1_E:				return "EAC3_7_1_E"
		case kAudioChannelLayoutTag_EAC3_7_1_F:				return "EAC3_7_1_F"
		case kAudioChannelLayoutTag_EAC3_7_1_G:				return "EAC3_7_1_G"
		case kAudioChannelLayoutTag_EAC3_7_1_H:				return "EAC3_7_1_H"
		case kAudioChannelLayoutTag_DTS_3_1:				return "DTS_3_1"
		case kAudioChannelLayoutTag_DTS_4_1:				return "DTS_4_1"
		case kAudioChannelLayoutTag_DTS_6_0_A:				return "DTS_6_0_A"
		case kAudioChannelLayoutTag_DTS_6_0_B:				return "DTS_6_0_B"
		case kAudioChannelLayoutTag_DTS_6_0_C:				return "DTS_6_0_C"
		case kAudioChannelLayoutTag_DTS_6_1_A:				return "DTS_6_1_A"
		case kAudioChannelLayoutTag_DTS_6_1_B:				return "DTS_6_1_B"
		case kAudioChannelLayoutTag_DTS_6_1_C:				return "DTS_6_1_C"
		case kAudioChannelLayoutTag_DTS_7_0:				return "DTS_7_0"
		case kAudioChannelLayoutTag_DTS_7_1:				return "DTS_7_1"
		case kAudioChannelLayoutTag_DTS_8_0_A:				return "DTS_8_0_A"
		case kAudioChannelLayoutTag_DTS_8_0_B:				return "DTS_8_0_B"
		case kAudioChannelLayoutTag_DTS_8_1_A:				return "DTS_8_1_A"
		case kAudioChannelLayoutTag_DTS_8_1_B:				return "DTS_8_1_B"
		case kAudioChannelLayoutTag_DTS_6_1_D:				return "DTS_6_1_D"
		case kAudioChannelLayoutTag_WAVE_4_0_B:				return "WAVE_4_0_B"
		case kAudioChannelLayoutTag_WAVE_5_0_B:				return "WAVE_5_0_B"
		case kAudioChannelLayoutTag_WAVE_5_1_B:				return "WAVE_5_1_B"
		case kAudioChannelLayoutTag_WAVE_6_1:				return "WAVE_6_1"
		case kAudioChannelLayoutTag_WAVE_7_1:				return "WAVE_7_1"
		case kAudioChannelLayoutTag_Atmos_5_1_2:			return "Atmos_5_1_2"
		case kAudioChannelLayoutTag_Atmos_5_1_4:			return "Atmos_5_1_4"
		case kAudioChannelLayoutTag_Atmos_7_1_2:			return "Atmos_7_1_2"
		case kAudioChannelLayoutTag_Atmos_7_1_4:			return "Atmos_7_1_4"
		case kAudioChannelLayoutTag_Atmos_9_1_6:			return "Atmos_9_1_6"
		default: 											break

		}

		switch (self & 0xFFFF0000) {
		case kAudioChannelLayoutTag_HOA_ACN_SN3D:			return "HOA_ACN_SN3D"
		case kAudioChannelLayoutTag_HOA_ACN_N3D:			return "HOA_ACN_N3D"
		case kAudioChannelLayoutTag_DiscreteInOrder:		return "Discrete In Order"
		case kAudioChannelLayoutTag_Unknown:				return "Unknown"
		default: 											break
		}

		return "0x\(String(self, radix: 16, uppercase: false))"
	}
}

extension AudioChannelLabel {
	/// Returns the name of the `AudioChannelLabel`value of `self`
	var channelLabelName: String {
		switch self {
		case kAudioChannelLabel_Unknown:					return "Unknown"
		case kAudioChannelLabel_Unused:						return "Unused"
		case kAudioChannelLabel_UseCoordinates:				return "Use Coordinates"
		case kAudioChannelLabel_Left:						return "Left"
		case kAudioChannelLabel_Right:						return "Right"
		case kAudioChannelLabel_Center:						return "Center"
		case kAudioChannelLabel_LFEScreen:					return "LFE Screen"
		case kAudioChannelLabel_LeftSurround:				return "Left Surround"
		case kAudioChannelLabel_RightSurround:				return "Right Surround"
		case kAudioChannelLabel_LeftCenter:					return "Left Center"
		case kAudioChannelLabel_RightCenter:				return "Right Center"
		case kAudioChannelLabel_CenterSurround:				return "Center Surround"
		case kAudioChannelLabel_LeftSurroundDirect:			return "Left Surround Direct"
		case kAudioChannelLabel_RightSurroundDirect:		return "Right Surround Direct"
		case kAudioChannelLabel_TopCenterSurround:			return "Top Center Surround"
		case kAudioChannelLabel_VerticalHeightLeft:			return "Vertical Height Left"
		case kAudioChannelLabel_VerticalHeightCenter:		return "Vertical Height Center"
		case kAudioChannelLabel_VerticalHeightRight:		return "Vertical Height Right"
		case kAudioChannelLabel_TopBackLeft:				return "Top Back Left"
		case kAudioChannelLabel_TopBackCenter:				return "Top Back Center"
		case kAudioChannelLabel_TopBackRight:				return "Top Back Right"
		case kAudioChannelLabel_RearSurroundLeft:			return "Rear Surround Left"
		case kAudioChannelLabel_RearSurroundRight:			return "Rear Surround Right"
		case kAudioChannelLabel_LeftWide:					return "Left Wide"
		case kAudioChannelLabel_RightWide:					return "Right Wide"
		case kAudioChannelLabel_LFE2:						return "LFE2"
		case kAudioChannelLabel_LeftTotal:					return "Left Total"
		case kAudioChannelLabel_RightTotal:					return "Right Total"
		case kAudioChannelLabel_HearingImpaired:			return "Hearing Impaired"
		case kAudioChannelLabel_Narration:					return "Narration"
		case kAudioChannelLabel_Mono:						return "Mono"
		case kAudioChannelLabel_DialogCentricMix:			return "Dialog Centric Mix"
		case kAudioChannelLabel_CenterSurroundDirect:		return "Center Surround Direct"
		case kAudioChannelLabel_Haptic:						return "Haptic"
		case kAudioChannelLabel_LeftTopMiddle:				return "Left Top Middle"
		case kAudioChannelLabel_RightTopMiddle:				return "Right Top Middle"
		case kAudioChannelLabel_LeftTopRear:				return "Left Top Rear"
		case kAudioChannelLabel_CenterTopRear:				return "Center Top Rear"
		case kAudioChannelLabel_RightTopRear:				return "Right Top Rear"
		case kAudioChannelLabel_Ambisonic_W:				return "Ambisonic_W"
		case kAudioChannelLabel_Ambisonic_X:				return "Ambisonic_X"
		case kAudioChannelLabel_Ambisonic_Y:				return "Ambisonic_Y"
		case kAudioChannelLabel_Ambisonic_Z:				return "Ambisonic_Z"
		case kAudioChannelLabel_MS_Mid:						return "MS_Mid"
		case kAudioChannelLabel_MS_Side:					return "MS_Side"
		case kAudioChannelLabel_XY_X:						return "XY_X"
		case kAudioChannelLabel_XY_Y:						return "XY_Y"
		case kAudioChannelLabel_BinauralLeft:				return "Binaural Left"
		case kAudioChannelLabel_BinauralRight:				return "Binaural Right"
		case kAudioChannelLabel_HeadphonesLeft:				return "Headphones Left"
		case kAudioChannelLabel_HeadphonesRight:			return "Headphones Right"
		case kAudioChannelLabel_ClickTrack:					return "Click Track"
		case kAudioChannelLabel_ForeignLanguage:			return "Foreign Language"
		case kAudioChannelLabel_Discrete:					return "Discrete"
		case kAudioChannelLabel_Discrete_0:					return "Discrete_0"
		case kAudioChannelLabel_Discrete_1:					return "Discrete_1"
		case kAudioChannelLabel_Discrete_2:					return "Discrete_2"
		case kAudioChannelLabel_Discrete_3:					return "Discrete_3"
		case kAudioChannelLabel_Discrete_4:					return "Discrete_4"
		case kAudioChannelLabel_Discrete_5:					return "Discrete_5"
		case kAudioChannelLabel_Discrete_6:					return "Discrete_6"
		case kAudioChannelLabel_Discrete_7:					return "Discrete_7"
		case kAudioChannelLabel_Discrete_8:					return "Discrete_8"
		case kAudioChannelLabel_Discrete_9:					return "Discrete_9"
		case kAudioChannelLabel_Discrete_10:				return "Discrete_10"
		case kAudioChannelLabel_Discrete_11:				return "Discrete_11"
		case kAudioChannelLabel_Discrete_12:				return "Discrete_12"
		case kAudioChannelLabel_Discrete_13:				return "Discrete_13"
		case kAudioChannelLabel_Discrete_14:				return "Discrete_14"
		case kAudioChannelLabel_Discrete_15:				return "Discrete_15"
		case kAudioChannelLabel_Discrete_65535:				return "Discrete_65535"
		case kAudioChannelLabel_HOA_ACN:					return "HOA_ACN"
		case kAudioChannelLabel_HOA_ACN_0:					return "HOA_ACN_0"
		case kAudioChannelLabel_HOA_ACN_1:					return "HOA_ACN_1"
		case kAudioChannelLabel_HOA_ACN_2:					return "HOA_ACN_2"
		case kAudioChannelLabel_HOA_ACN_3:					return "HOA_ACN_3"
		case kAudioChannelLabel_HOA_ACN_4:					return "HOA_ACN_4"
		case kAudioChannelLabel_HOA_ACN_5:					return "HOA_ACN_5"
		case kAudioChannelLabel_HOA_ACN_6:					return "HOA_ACN_6"
		case kAudioChannelLabel_HOA_ACN_7:					return "HOA_ACN_7"
		case kAudioChannelLabel_HOA_ACN_8:					return "HOA_ACN_8"
		case kAudioChannelLabel_HOA_ACN_9:					return "HOA_ACN_9"
		case kAudioChannelLabel_HOA_ACN_10:					return "HOA_ACN_10"
		case kAudioChannelLabel_HOA_ACN_11:					return "HOA_ACN_11"
		case kAudioChannelLabel_HOA_ACN_12:					return "HOA_ACN_12"
		case kAudioChannelLabel_HOA_ACN_13:					return "HOA_ACN_13"
		case kAudioChannelLabel_HOA_ACN_14:					return "HOA_ACN_14"
		case kAudioChannelLabel_HOA_ACN_15:					return "HOA_ACN_15"
		case kAudioChannelLabel_HOA_ACN_65024:				return "HOA_ACN_65024"
		default:											break
		}

		return "0x\(String(self, radix: 16, uppercase: false))"
	}
}

// From https://stackoverflow.com/questions/32102936/how-do-you-enumerate-optionsettype-in-swift
extension OptionSet where RawValue: FixedWidthInteger {
	/// Returns a sequence containing the individual bits of `self`
	func elements() -> AnySequence<Self> {
		var remainingBits = rawValue
		var bitMask: RawValue = 1
		return AnySequence {
			return AnyIterator {
				while remainingBits != 0 {
					defer { bitMask = bitMask << 1 }
					if remainingBits & bitMask != 0 {
						remainingBits = remainingBits & ~bitMask
						return Self(rawValue: bitMask)
					}
				}
				return nil
			}
		}
	}
}

extension AudioChannelBitmap {
	/// Returns the names of the channel bits in `self`
	var bitmapDescription: String {
		var result = [String]()
		for bit in elements() {
			switch bit {
			case .bit_Left: 					result.append("Left")
			case .bit_Right: 					result.append("Right")
			case .bit_Center: 					result.append("Center")
			case .bit_LFEScreen: 				result.append("LFE Screen")
			case .bit_LeftSurround: 			result.append("Left Surround")
			case .bit_RightSurround: 			result.append("Right Surround")
			case .bit_LeftCenter: 				result.append("Left Center")
			case .bit_RightCenter: 				result.append("Right Center")
			case .bit_CenterSurround: 			result.append("Center Surround")
			case .bit_LeftSurroundDirect: 		result.append("Left Surround Direct")
			case .bit_RightSurroundDirect: 		result.append("Right Surround Direct")
			case .bit_TopCenterSurround: 		result.append("Top Center Surround")
			case .bit_VerticalHeightLeft: 		result.append("Vertical Height Left")
			case .bit_VerticalHeightCenter: 	result.append("Vertical Height Center")
			case .bit_VerticalHeightRight: 		result.append("Vertical Height Right")
			case .bit_TopBackLeft: 				result.append("Top Back Left")
			case .bit_TopBackCenter: 			result.append("Top Back Center")
			case .bit_TopBackRight: 			result.append("Top Back Right")
			case .bit_LeftTopFront: 			result.append("Left Top Front")
			case .bit_CenterTopFront: 			result.append("Center Top Front")
			case .bit_RightTopFront: 			result.append("Right Top Front")
			case .bit_LeftTopMiddle: 			result.append("Left Top Middle")
			case .bit_CenterTopMiddle: 			result.append("Center Top Middle")
			case .bit_RightTopMiddle: 			result.append("Right Top Middle")
			case .bit_LeftTopRear: 				result.append("Left Top Rear")
			case .bit_CenterTopRear: 			result.append("Center Top Rear")
			case .bit_RightTopRear: 			result.append("Right Top Rear")
			default: 							result.append("0x\(String(bit.rawValue, radix: 16, uppercase: false))")
			}
		}
		return result.joined(separator: ", ")
	}
}

extension AudioChannelFlags {
	/// Returns the names of the flags in `self`
	public var channelFlagsDescription: String {
		var result = [String]()
		for bit in elements() {
			switch bit {
			case .rectangularCoordinates: 	result.append("Rectangular Coordinates")
			case .sphericalCoordinates: 	result.append("Spherical Coordinates")
			case .meters: 					result.append("Meters")
			default: 						result.append("0x\(String(bit.rawValue, radix: 16, uppercase: false))")
			}
		}
		return result.joined(separator: ", ")
	}
}

extension AudioBuffer: CustomDebugStringConvertible {
	public var debugDescription: String {
		return "<\(type(of: self)): mNumberChannels = \(mNumberChannels), mDataByteSize = \(mDataByteSize), mData = \(mData?.debugDescription ?? "nil")>"
	}
}
