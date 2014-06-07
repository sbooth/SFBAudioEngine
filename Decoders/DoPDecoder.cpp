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

#include <algorithm>

#include "DoPDecoder.h"
#include "CFErrorUtilities.h"
#include "Logger.h"

SFB::Audio::DoPDecoder::DoPDecoder(Decoder::unique_ptr decoder)
	: mDecoder(std::move(decoder)), mMarkerFlag(false)
{
	assert(nullptr != mDecoder);
}

bool SFB::Audio::DoPDecoder::_Open(CFErrorRef *error)
{
	if(!mDecoder->IsOpen() && !mDecoder->Open(error))
		return false;

	const auto& decoderFormat = mDecoder->GetFormat();

	if(!decoderFormat.IsDSD()) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSD file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not a DSD file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, GetURL(), failureReason, recoverySuggestion);
		}
		
		return false;
	}

	if(28224000 != decoderFormat.mSampleRate) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.DOP", "Unsupported sample rate: " << decoderFormat.mSampleRate);

		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not supported."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Unsupported DSD sample rate"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's sample rate is not supported for DSD over PCM."), "");

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	mBufferList.Allocate(decoderFormat, 1024);

	// Generate interleaved 24 bit output
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;

	mFormat.mSampleRate			= 176400;
	mFormat.mChannelsPerFrame	= decoderFormat.mChannelsPerFrame;
	mFormat.mBitsPerChannel		= 24;

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	return true;
}

bool SFB::Audio::DoPDecoder::_Close(CFErrorRef *error)
{
	if(!mDecoder->Close(error))
		return false;

	mBufferList.Deallocate();

	return true;
}

SFB::CFString SFB::Audio::DoPDecoder::_GetSourceFormatDescription() const
{
	return mDecoder->CreateSourceFormatDescription();
}

#pragma mark Functionality

UInt32 SFB::Audio::DoPDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	// Only multiples of 8 frames can be read (8 frames equals one byte)
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 != frameCount % 8) {
		LOGGER_WARNING("org.sbooth.AudioEngine.Decoder.DOP", "_ReadAudio() called with invalid parameters");
		return 0;
	}

	UInt32 framesRead = 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	for(;;) {
		// Grab the DSD audio
		UInt32 framesDecoded = mDecoder->ReadAudio(mBufferList, std::min(mBufferList.GetCapacityFrames(), frameCount - framesRead));
		if(0 == framesDecoded)
			break;

		// Convert to DoP
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			const unsigned char *src = (const unsigned char *)mBufferList->mBuffers[i].mData;
			unsigned char *dst = (unsigned char *)bufferList->mBuffers[i].mData + bufferList->mBuffers[i].mDataByteSize;

			for(UInt32 j = 0; j < framesDecoded; ++j) {
				// Insert the DSD marker
				*dst++ = mMarkerFlag ? 0xfa : 0x05;

				// Copy the DSD bits
				*dst++ = *src++;
				*dst++ = *src++;

				mMarkerFlag = !mMarkerFlag;
			}

			bufferList->mBuffers[i].mDataByteSize += mFormat.FrameCountToByteCount(framesDecoded);
		}

		framesRead += framesDecoded;

		// All requested frames were read
		if(framesRead == frameCount)
			break;
	}

	return framesRead;
}

SInt64 SFB::Audio::DoPDecoder::_SeekToFrame(SInt64 frame)
{
	if(-1 == mDecoder->SeekToFrame(frame))
		return -1;

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = 0;

	return _GetCurrentFrame();
}
