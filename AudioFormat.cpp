/*
 * Copyright (c) 2014 - 2018 Stephen F. Booth <me@sbooth.org>
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
