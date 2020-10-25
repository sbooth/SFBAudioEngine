/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "AudioFormat.h"

namespace {

	static AudioFormatFlags CalculateLPCMFlags(UInt32 validBitsPerChannel, UInt32 totalBitsPerChannel, bool isFloat, bool isBigEndian, bool isNonInterleaved)
	{
		return (isFloat ? kAudioFormatFlagIsFloat : kAudioFormatFlagIsSignedInteger) | (isBigEndian ? ((UInt32)kAudioFormatFlagIsBigEndian) : 0) | ((validBitsPerChannel == totalBitsPerChannel) ? kAudioFormatFlagIsPacked : kAudioFormatFlagIsAlignedHigh) | (isNonInterleaved ? ((UInt32)kAudioFormatFlagIsNonInterleaved) : 0);
	}

	static void FillOutASBDForLPCM(AudioStreamBasicDescription *asbd, Float64 sampleRate, UInt32 channelsPerFrame, UInt32 validBitsPerChannel, UInt32 totalBitsPerChannel, bool isFloat, bool isBigEndian, bool isNonInterleaved)
	{
		asbd->mFormatID = kAudioFormatLinearPCM;
		asbd->mFormatFlags = CalculateLPCMFlags(validBitsPerChannel, totalBitsPerChannel, isFloat, isBigEndian, isNonInterleaved);

		asbd->mSampleRate = sampleRate;
		asbd->mChannelsPerFrame = channelsPerFrame;
		asbd->mBitsPerChannel = validBitsPerChannel;

		asbd->mBytesPerPacket = (isNonInterleaved ? 1 : channelsPerFrame) * (totalBitsPerChannel / 8);
		asbd->mFramesPerPacket = 1;
		asbd->mBytesPerFrame = (isNonInterleaved ? 1 : channelsPerFrame) * (totalBitsPerChannel / 8);
	}

}

SFB::Audio::Format::Format(CommonPCMFormat format, Float32 sampleRate, UInt32 channelsPerFrame, bool isInterleaved)
{
	assert(0 < sampleRate);
	assert(0 < channelsPerFrame);

	memset(this, 0, sizeof(AudioStreamBasicDescription));

	switch(format) {
		case kCommonPCMFormatFloat32:
			FillOutASBDForLPCM(this, sampleRate, channelsPerFrame, 32, 32, true, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !isInterleaved);
			break;
		case kCommonPCMFormatFloat64:
			FillOutASBDForLPCM(this, sampleRate, channelsPerFrame, 64, 64, true, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !isInterleaved);
			break;
		case kCommonPCMFormatInt16:
			FillOutASBDForLPCM(this, sampleRate, channelsPerFrame, 16, 16, false, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !isInterleaved);
			break;
		case kCommonPCMFormatInt32:
			FillOutASBDForLPCM(this, sampleRate, channelsPerFrame, 32, 32, false, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !isInterleaved);
			break;
	}
}

size_t SFB::Audio::Format::FrameCountToByteCount(size_t frameCount) const
{
	switch(mFormatID) {
		case kAudioFormatDirectStreamDigital:
			return frameCount / 8;

		case kAudioFormatDoP:
		case kAudioFormatLinearPCM:
			return frameCount * mBytesPerFrame;

		default:
			return 0;
	}
}

size_t SFB::Audio::Format::ByteCountToFrameCount(size_t byteCount) const
{
	switch(mFormatID) {
		case kAudioFormatDirectStreamDigital:
			return byteCount * 8;

		case kAudioFormatDoP:
		case kAudioFormatLinearPCM:
			return byteCount / mBytesPerFrame;

		default:
			return 0;
	}
}

bool SFB::Audio::Format::GetNonInterleavedEquivalent(Format& format) const
{
	if(!IsPCM())
		return false;

	format = *this;

	if(IsInterleaved()) {
		format.mFormatFlags |= kAudioFormatFlagIsNonInterleaved;
		format.mBytesPerPacket /= mChannelsPerFrame;
		format.mBytesPerFrame /= mChannelsPerFrame;
	}

	return true;
}

bool SFB::Audio::Format::GetInterleavedEquivalent(Format& format) const
{
	if(!IsPCM())
		return false;

	format = *this;

	if(!IsInterleaved()) {
		format.mFormatFlags &= ~kAudioFormatFlagIsNonInterleaved;
		format.mBytesPerPacket *= mChannelsPerFrame;
		format.mBytesPerFrame *= mChannelsPerFrame;
	}

	return true;
}

bool SFB::Audio::Format::GetStandardEquivalent(Format& format) const
{
	if(!IsPCM())
		return false;

	FillOutASBDForLPCM(&format, mSampleRate, mChannelsPerFrame, 32, 32, true, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, true);
	
	return true;
}

// Most of this is stolen from Apple's CAStreamBasicDescription::Print()
SFB::CFString SFB::Audio::Format::Description() const
{
	CFMutableString result{ CFStringCreateMutable(kCFAllocatorDefault, 0) };

	unsigned char formatID [5];
	*(UInt32 *)formatID = OSSwapHostToBigInt32(mFormatID);
	formatID[4] = '\0';

	// General description
	CFStringAppendFormat(result, NULL, CFSTR("%u ch, %.2f Hz, '%.4s' (0x%0.8x) "), mChannelsPerFrame, mSampleRate, formatID, mFormatFlags);

	if(kAudioFormatLinearPCM == mFormatID) {
		// Bit depth
		UInt32 fractionalBits = ((0x3f << 7)/*kLinearPCMFormatFlagsSampleFractionMask*/ & mFormatFlags) >> 7/*kLinearPCMFormatFlagsSampleFractionShift*/;
		if(0 < fractionalBits)
			CFStringAppendFormat(result, NULL, CFSTR("%d.%d-bit"), mBitsPerChannel - fractionalBits, fractionalBits);
		else
			CFStringAppendFormat(result, NULL, CFSTR("%d-bit"), mBitsPerChannel);

		// Endianness
		bool isInterleaved = !(kAudioFormatFlagIsNonInterleaved & mFormatFlags);
		UInt32 interleavedChannelCount = (isInterleaved ? mChannelsPerFrame : 1);
		UInt32 sampleSize = (0 < mBytesPerFrame && 0 < interleavedChannelCount ? mBytesPerFrame / interleavedChannelCount : 0);
		if(1 < sampleSize)
			CFStringAppend(result, (kLinearPCMFormatFlagIsBigEndian & mFormatFlags) ? CFSTR(" big-endian") : CFSTR(" little-endian"));

		// Sign
		bool isInteger = !(kLinearPCMFormatFlagIsFloat & mFormatFlags);
		if(isInteger)
			CFStringAppend(result, (kLinearPCMFormatFlagIsSignedInteger & mFormatFlags) ? CFSTR(" signed") : CFSTR(" unsigned"));

		// Integer or floating
		CFStringAppend(result, isInteger ? CFSTR(" integer") : CFSTR(" float"));

		// Packedness
		if(0 < sampleSize && ((sampleSize << 3) != mBitsPerChannel))
			CFStringAppendFormat(result, NULL, (kLinearPCMFormatFlagIsPacked & mFormatFlags) ? CFSTR(", packed in %d bytes") : CFSTR(", unpacked in %d bytes"), sampleSize);

		// Alignment
		if((0 < sampleSize && ((sampleSize << 3) != mBitsPerChannel)) || (0 != (mBitsPerChannel & 7)))
			CFStringAppend(result, (kLinearPCMFormatFlagIsAlignedHigh & mFormatFlags) ? CFSTR(" high-aligned") : CFSTR(" low-aligned"));

		if(!isInterleaved)
			CFStringAppend(result, CFSTR(", deinterleaved"));
	}
	else if(kAudioFormatAppleLossless == mFormatID) {
		UInt32 sourceBitDepth = 0;
		switch(mFormatFlags) {
			case kAppleLosslessFormatFlag_16BitSourceData:		sourceBitDepth = 16;	break;
			case kAppleLosslessFormatFlag_20BitSourceData:		sourceBitDepth = 20;	break;
			case kAppleLosslessFormatFlag_24BitSourceData:		sourceBitDepth = 24;	break;
			case kAppleLosslessFormatFlag_32BitSourceData:		sourceBitDepth = 32;	break;
		}

		if(0 != sourceBitDepth)
			CFStringAppendFormat(result, NULL, CFSTR("from %d-bit source, "), sourceBitDepth);
		else
			CFStringAppend(result, CFSTR("from UNKNOWN source bit depth, "));

		CFStringAppendFormat(result, NULL, CFSTR(" %d frames/packet"), mFramesPerPacket);
	}
	else
		CFStringAppendFormat(result, NULL, CFSTR("%u bits/channel, %u bytes/packet, %u frames/packet, %u bytes/frame"), mBitsPerChannel, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame);

	return CFString((CFStringRef)result.Relinquish());
}
