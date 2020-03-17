/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "AudioFormat.h"

SFB::Audio::AudioFormat::AudioFormat()
{
    memset(this, 0, sizeof(AudioStreamBasicDescription));
}

SFB::Audio::AudioFormat::AudioFormat(const AudioStreamBasicDescription& format)
{
    memcpy(this, &format, sizeof(AudioStreamBasicDescription));
}

SFB::Audio::AudioFormat::AudioFormat(const AudioFormat& rhs)
{
	memcpy(this, &rhs, sizeof(AudioStreamBasicDescription));
}

SFB::Audio::AudioFormat& SFB::Audio::AudioFormat::operator=(const AudioFormat& rhs)
{
	memcpy(this, &rhs, sizeof(AudioStreamBasicDescription));
	return *this;
}

// Although wildcards are allowed (0 and '****'), they aren't handled here
bool SFB::Audio::AudioFormat::operator==(const AudioFormat& rhs) const
{
	if(mSampleRate != rhs.mSampleRate)
		return false;

	if(mFormatID != rhs.mFormatID)
		return false;

	if(mFormatFlags != rhs.mFormatFlags)
		return false;

	if(mBytesPerPacket != rhs.mBytesPerPacket)
		return false;

	if(mFramesPerPacket != rhs.mFramesPerPacket)
		return false;

	if(mBytesPerFrame != rhs.mBytesPerFrame)
		return false;

	if(mChannelsPerFrame != rhs.mChannelsPerFrame)
		return false;

	if(mBitsPerChannel != rhs.mBitsPerChannel)
		return false;

	return true;
}

bool SFB::Audio::AudioFormat::IsInterleaved() const
{
	return !(kAudioFormatFlagIsNonInterleaved & mFormatFlags);
}

bool SFB::Audio::AudioFormat::IsPCM() const
{
	return kAudioFormatLinearPCM == mFormatID;
}

bool SFB::Audio::AudioFormat::IsDSD() const
{
	return kAudioFormatDirectStreamDigital == mFormatID;
}

bool SFB::Audio::AudioFormat::IsDoP() const
{
	return kAudioFormatDoP == mFormatID;
}

bool SFB::Audio::AudioFormat::IsBigEndian() const
{
	return kAudioFormatFlagIsBigEndian & mFormatFlags;
}

bool SFB::Audio::AudioFormat::IsNativeEndian() const
{
	return kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mFormatFlags);
}

size_t SFB::Audio::AudioFormat::FrameCountToByteCount(size_t frameCount) const
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

size_t SFB::Audio::AudioFormat::ByteCountToFrameCount(size_t byteCount) const
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

// Most of this is stolen from Apple's CAStreamBasicDescription::Print()
SFB::CFString SFB::Audio::AudioFormat::Description() const
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
