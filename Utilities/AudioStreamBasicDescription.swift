//
// Copyright (c) 2006 - 2021 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import Foundation
import CoreAudio

extension AudioStreamBasicDescription {

	// MARK: Common Formats

	/// Common PCM formats
	public enum CommonPCMFormat {
		/// Native endian 32-bit floating point samples
		/// - remark: This corresponds to `Float`
		case float32
		/// Native endian 64-bit floating point samples
		/// - remark: This corresponds to `Double`
		case float64
		/// Native endian signed 16-bit integer samples
		/// - remark: This corresponds to `Int16`
		case int16
		/// Native endian signed 32-bit integer samples
		/// - remark: This corresponds to `Int32`
		case int32
	}

	/// Initializes an `AudioStreamBasicDescription` for a common PCM variant
	/// - parameter format: The desired common PCM variant
	/// - parameter sampleRate: The audio sample rate
	/// - parameter channelsPerFrame: The number of audio channels
	/// - parameter isInterleaved: Whether the audio samples are interleaved
	public init(commonFormat format: CommonPCMFormat, sampleRate: Float64, channelsPerFrame: UInt32, isInterleaved interleaved: Bool) {
		switch format {
		case .float32:
			self = makeASBDForLinearPCM(sampleRate: sampleRate, channelsPerFrame: channelsPerFrame, validBitsPerChannel: 32, totalBitsPerChannel: 32, isFloat: true, isBigEndian: kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, isNonInterleaved: !interleaved)
		case .float64:
			self = makeASBDForLinearPCM(sampleRate: sampleRate, channelsPerFrame: channelsPerFrame, validBitsPerChannel: 64, totalBitsPerChannel: 64, isFloat: true, isBigEndian: kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, isNonInterleaved: !interleaved)
		case .int16:
			self = makeASBDForLinearPCM(sampleRate: sampleRate, channelsPerFrame: channelsPerFrame, validBitsPerChannel: 16, totalBitsPerChannel: 16, isFloat: false, isBigEndian: kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, isNonInterleaved: !interleaved)
		case .int32:
			self = makeASBDForLinearPCM(sampleRate: sampleRate, channelsPerFrame: channelsPerFrame, validBitsPerChannel: 32, totalBitsPerChannel: 32, isFloat: false, isBigEndian: kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, isNonInterleaved: !interleaved)
		}
	}

	// MARK: Format Information

	/// Returns `true` if this format is non-interleaved
	public var isNonInterleaved: Bool {
		return mFormatFlags & kAudioFormatFlagIsNonInterleaved == kAudioFormatFlagIsNonInterleaved
	}

	/// Returns `true` if this format is interleaved
	public var isInterleaved: Bool {
		return mFormatFlags & kAudioFormatFlagIsNonInterleaved == 0
	}

	/// Returns the number of interleaved channels
	public var interleavedChannelCount: UInt32 {
		return isInterleaved ? mChannelsPerFrame : 1
	}

	/// Returns the number of channel streams
	public var channelStreamCount: UInt32 {
		return isInterleaved ? 1 : mChannelsPerFrame
	}

	/// Returns the number of channels
	public var channelCount: UInt32 {
		return mChannelsPerFrame
	}

	/// Returns `true` if this format is linear PCM
	public var isPCM: Bool {
		return mFormatID == kAudioFormatLinearPCM
	}

	/// Returns `true` if this format is big-endian
	public var isBigEndian: Bool {
		return mFormatFlags & kAudioFormatFlagIsBigEndian == kAudioFormatFlagIsBigEndian
	}

	/// Returns `true` if this format is little-endian
	public var isLittleEndian: Bool {
		return mFormatFlags & kAudioFormatFlagIsBigEndian == 0
	}

	/// Returns `true` if this format is native-endian
	public var isNativeEndian: Bool {
		return mFormatFlags & kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian
	}

	/// Returns `true` if this format is floating-point linear PCM
	public var isFloat: Bool {
		return isPCM && mFormatFlags & kAudioFormatFlagIsFloat == kAudioFormatFlagIsFloat
	}

	/// Returns `true` if this format is integer linear PCM
	public var isInteger: Bool {
		return isPCM && mFormatFlags & kAudioFormatFlagIsFloat == 0
	}

	/// Returns `true` if this format is signed integer linear PCM
	public var isSignedInteger: Bool {
		return isPCM && mFormatFlags & kAudioFormatFlagIsSignedInteger == kAudioFormatFlagIsSignedInteger
	}

	/// Returns `true` if this format is packed
	public var isPacked: Bool {
		return mFormatFlags & kAudioFormatFlagIsPacked == kAudioFormatFlagIsPacked
	}

	/// Returns `true` if this format is high-aligned
	public var isAlignedHigh: Bool {
		return mFormatFlags & kAudioFormatFlagIsAlignedHigh == kAudioFormatFlagIsAlignedHigh
	}

	/// Returns `true` if this format is non-mixable
	/// - note: This flag is only used when interacting with HAL stream formats
	public var isNonMixable: Bool {
		return mFormatFlags & kAudioFormatFlagIsNonMixable == kAudioFormatFlagIsNonMixable
	}

	/// Returns `true` if this format is mixable
	/// - note: This flag is only used when interacting with HAL stream formats
	public var isMixable: Bool {
		return isPCM && mFormatFlags & kAudioFormatFlagIsNonMixable == 0
	}

	/// Returns the sample word size in bytes
	public var sampleWordSize: Int {
		let interleavedChannelCount = self.interleavedChannelCount
//		assert(interleavedChannelCount != 0, "self.interleavedChannelCount == 0 in sampleWordSize")
		if(interleavedChannelCount == 0) {
			return 0
		}
		return Int(mBytesPerFrame / interleavedChannelCount)
	}

	/// Returns the byte size of `frameCount` audio frames
	/// - note: This is equivalent to `frameCount * mBytesPerFrame`
	public func byteSize(forFrameCount frameCount: Int) -> Int {
		return frameCount * Int(mBytesPerFrame)
	}

	/// Returns the frame count of `byteSize` bytes
	/// - note: This is equivalent to `byteSize / mBytesPerFrame`
	public func frameCount(forByteSize byteSize: Int) -> Int {
//		assert(mBytesPerFrame != 0, "mBytesPerFrame == 0 in frameCount(forByteSize:)")
		if(mBytesPerFrame == 0) {
			return 0
		}
		return byteSize / Int(mBytesPerFrame)
	}

	// MARK: Format Transformation

	/// Returns the equivalent non-interleaved format of `self`
	/// - note: This returns `nil` for non-PCM formats
	public func nonInterleavedEquivalent() -> AudioStreamBasicDescription? {
		guard isPCM else {
			return nil
		}

		var format = self
		if isInterleaved {
			format.mFormatFlags |= kAudioFormatFlagIsNonInterleaved
			format.mBytesPerPacket /= mChannelsPerFrame
			format.mBytesPerFrame /= mChannelsPerFrame
		}
		return format
	}

	/// Returns the equivalent interleaved format of `self`
	/// - note: This returns `nil` for non-PCM formats
	public func interleavedEquivalent() -> AudioStreamBasicDescription? {
		guard isPCM else {
			return nil
		}

		var format = self
		if !isInterleaved {
			format.mFormatFlags &= ~kAudioFormatFlagIsNonInterleaved;
			format.mBytesPerPacket *= mChannelsPerFrame;
			format.mBytesPerFrame *= mChannelsPerFrame;
		}
		return format
	}

	/// Returns the equivalent standard format of `self`
	/// - note: This returns `nil` for non-PCM formats
	public func standardEquivalent() -> AudioStreamBasicDescription? {
		guard isPCM else {
			return nil
		}
		return makeASBDForLinearPCM(sampleRate: mSampleRate, channelsPerFrame: mChannelsPerFrame, validBitsPerChannel: 32, totalBitsPerChannel: 32, isFloat: true, isBigEndian: kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, isNonInterleaved: true)
	}

	/// Resets `self` to the default state
	public mutating func reset() {
		memset(&self, 0, MemoryLayout<AudioStreamBasicDescription>.stride);
	}

}

extension AudioStreamBasicDescription: Equatable {
	public static func == (lhs: AudioStreamBasicDescription, rhs: AudioStreamBasicDescription) -> Bool {
		return
			lhs.mFormatID == rhs.mFormatID &&
			lhs.mFormatFlags == rhs.mFormatFlags &&
			lhs.mSampleRate == rhs.mSampleRate &&
			lhs.mChannelsPerFrame == rhs.mChannelsPerFrame &&
			lhs.mBitsPerChannel == rhs.mBitsPerChannel &&
			lhs.mBytesPerPacket == rhs.mBytesPerPacket &&
			lhs.mFramesPerPacket == rhs.mFramesPerPacket &&
			lhs.mBytesPerFrame == rhs.mBytesPerFrame
	}
}

private func makeLinearPCMFlags(validBitsPerChannel: UInt32, totalBitsPerChannel: UInt32, isFloat float: Bool, isBigEndian bigEndian: Bool, isNonInterleaved nonInterleaved: Bool) -> AudioFormatFlags
{
	return (float ? kAudioFormatFlagIsFloat : kAudioFormatFlagIsSignedInteger) | (bigEndian ? kAudioFormatFlagIsBigEndian : 0) | ((validBitsPerChannel == totalBitsPerChannel) ? kAudioFormatFlagIsPacked : kAudioFormatFlagIsAlignedHigh) | (nonInterleaved ? kAudioFormatFlagIsNonInterleaved : 0);
}

private func makeASBDForLinearPCM(sampleRate: Float64, channelsPerFrame: UInt32, validBitsPerChannel: UInt32, totalBitsPerChannel: UInt32, isFloat float: Bool, isBigEndian bigEndian: Bool, isNonInterleaved nonInterleaved: Bool) -> AudioStreamBasicDescription
{
	var asbd = AudioStreamBasicDescription()

	asbd.mFormatID = kAudioFormatLinearPCM;
	asbd.mFormatFlags = makeLinearPCMFlags(validBitsPerChannel: validBitsPerChannel, totalBitsPerChannel: totalBitsPerChannel, isFloat: float, isBigEndian: bigEndian, isNonInterleaved: nonInterleaved);

	asbd.mSampleRate = sampleRate;
	asbd.mChannelsPerFrame = channelsPerFrame;
	asbd.mBitsPerChannel = validBitsPerChannel;

	asbd.mBytesPerPacket = (nonInterleaved ? 1 : channelsPerFrame) * (totalBitsPerChannel / 8);
	asbd.mFramesPerPacket = 1;
	asbd.mBytesPerFrame = (nonInterleaved ? 1 : channelsPerFrame) * (totalBitsPerChannel / 8);

	return asbd
}

extension AudioStreamBasicDescription: CustomDebugStringConvertible {
	public var debugDescription: String {
		// General description
		var result = String(format: "%u ch, %.2f Hz, '%@' (0x%0.8x) ", mChannelsPerFrame, mSampleRate, mFormatID.fourCC, mFormatFlags)

		if isPCM {
			// Bit depth
			let fractionalBits = (mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask) >> kLinearPCMFormatFlagsSampleFractionShift
			if fractionalBits > 0 {
				result.append(String(format: "%d.%d-bit", mBitsPerChannel - fractionalBits, fractionalBits))
			}
			else {
				result.append(String(format: "%d-bit", mBitsPerChannel))
			}

			// Endianness
			let sampleSize = mBytesPerFrame > 0 && interleavedChannelCount > 0 ? mBytesPerFrame / interleavedChannelCount : 0
			if sampleSize > 1 {
				result.append(isBigEndian ? " big-endian" : " little-endian")
			}

			// Sign
			if isInteger {
				result.append(isSignedInteger ? " signed" : " unsigned")
			}

			// Integer or floating
			result.append(isInteger ? " integer" : " float")

			// Packedness
			if sampleSize > 0 && ((sampleSize << 3) != mBitsPerChannel) {
				result.append(String(format: isPacked ? ", packed in %d bytes" : ", unpacked in %d bytes", sampleSize))
			}
			// Alignment
			if (sampleSize > 0 && ((sampleSize << 3) != mBitsPerChannel)) || ((mBitsPerChannel & 7) != 0) {
				result.append(isAlignedHigh ? " high-aligned" : " low-aligned")
			}

			if !isInterleaved {
				result.append(", deinterleaved")
			}
		}
		else if mFormatID == kAudioFormatAppleLossless {
			var sourceBitDepth: UInt32 = 0;
			switch mFormatFlags {
			case kAppleLosslessFormatFlag_16BitSourceData:		sourceBitDepth = 16
			case kAppleLosslessFormatFlag_20BitSourceData:		sourceBitDepth = 20
			case kAppleLosslessFormatFlag_24BitSourceData:		sourceBitDepth = 24
			case kAppleLosslessFormatFlag_32BitSourceData:		sourceBitDepth = 32
			default: 											break
			}

			if sourceBitDepth != 0 {
				result.append(String(format: "from %d-bit source, ", sourceBitDepth))
			}
			else {
				result.append("from UNKNOWN source bit depth, ")
			}

			result.append(String(format: " %d frames/packet", mFramesPerPacket))
		}
		else {
			result.append(String(format: "%u bits/channel, %u bytes/packet, %u frames/packet, %u bytes/frame", mBitsPerChannel, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame))
		}

		return result
	}
}
