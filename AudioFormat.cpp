/*
 *  Copyright (C) 2014 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
