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
//#include "CFErrorUtilities.h"
#include "Logger.h"

SFB::Audio::DoPDecoder::DoPDecoder(Decoder::unique_ptr decoder)
	: mDecoder(std::move(decoder))
{
	assert(nullptr != mDecoder);
}

bool SFB::Audio::DoPDecoder::_Open(CFErrorRef *error)
{
	if(!mDecoder->IsOpen() && !mDecoder->Open(error))
		return false;

	const auto& decoderFormat = mDecoder->GetFormat();

	if(!decoderFormat.IsDSD())
		return false;

	if(28224000 != decoderFormat.mSampleRate) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.DOP", "Unsupported sample rate: " << decoderFormat.mSampleRate);
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
		// Grab the DSD frames
		UInt32 framesDecoded = mDecoder->ReadAudio(mBufferList, std::min(mBufferList.GetCapacityFrames(), frameCount - framesRead));
		if(0 == framesDecoded)
			break;

		// Convert to DOP
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			const unsigned char *src = (const unsigned char *)mBufferList->mBuffers[i].mData;
			unsigned char *dst = (unsigned char *)bufferList->mBuffers[i].mData + bufferList->mBuffers[i].mDataByteSize;

			bufferList->mBuffers[i].mDataByteSize += mFormat.FrameCountToByteCount(framesDecoded);
		}

		framesRead += framesDecoded;

		// All requested frames were read
		if(framesRead == frameCount)
			break;

		UInt32	framesConverted	= (UInt32)(mFormat.ByteCountToFrameCount(bufferList->mBuffers[0].mDataByteSize));
	}

	for(;;) {
		UInt32	framesRemaining	= frameCount - framesRead;
		UInt32	framesToSkip	= (UInt32)(mFormat.ByteCountToFrameCount(bufferList->mBuffers[0].mDataByteSize));

		UInt32	framesInBuffer	= (UInt32)(mBufferList.GetFormat().ByteCountToFrameCount(mBufferList->mBuffers[0].mDataByteSize));
		UInt32	framesToCopy	= std::min(framesInBuffer, framesRemaining);

		// Copy data from the buffer to output
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			unsigned char *pullBuffer = (unsigned char *)bufferList->mBuffers[i].mData;
			memcpy(pullBuffer + (framesToSkip * mFormat.mBytesPerFrame), mBufferList->mBuffers[i].mData, framesToCopy * mFormat.mBytesPerFrame);
			bufferList->mBuffers[i].mDataByteSize += framesToCopy * mFormat.mBytesPerFrame;

			// Move remaining data in buffer to beginning
			if(framesToCopy != framesInBuffer) {
				pullBuffer = (unsigned char *)mBufferList->mBuffers[i].mData;
				memmove(pullBuffer, pullBuffer + (framesToCopy * mFormat.mBytesPerFrame), (framesInBuffer - framesToCopy) * mFormat.mBytesPerFrame);
			}

			mBufferList->mBuffers[i].mDataByteSize -= (UInt32)(framesToCopy * mFormat.mBytesPerFrame);
		}

		framesRead += framesToCopy;

		// All requested frames were read
		if(framesRead == frameCount)
			break;

		// Grab the next DSD frames
		UInt32 framesDecoded = mDecoder->ReadAudio(mBufferList, mBufferList.GetCapacityFrames());
		if(0 == framesDecoded)
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
