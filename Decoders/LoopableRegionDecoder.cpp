/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include "LoopableRegionDecoder.h"
#include "AudioDecoder.h"
#include "Logger.h"

LoopableRegionDecoder::LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame)
	: mDecoder(decoder), mStartingFrame(startingFrame), mFrameCount(0), mRepeatCount(0), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	assert(nullptr != decoder);
	assert(decoder->SupportsSeeking());
	
	mInputSource	= mDecoder->GetInputSource();
	mIsOpen			= mDecoder->IsOpen();

	if(mDecoder->IsOpen())
		SetupDecoder();
}

LoopableRegionDecoder::LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount)
	: mDecoder(decoder), mStartingFrame(startingFrame), mFrameCount(frameCount), mRepeatCount(0), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	assert(nullptr != decoder);
	assert(decoder->SupportsSeeking());

	mInputSource	= mDecoder->GetInputSource();
	mIsOpen			= mDecoder->IsOpen();

	if(mDecoder->IsOpen())
		SetupDecoder();
}

LoopableRegionDecoder::LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount)
	: mDecoder(decoder), mStartingFrame(startingFrame), mFrameCount(frameCount), mRepeatCount(repeatCount), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	assert(nullptr != decoder);
	assert(decoder->SupportsSeeking());
	
	mInputSource	= mDecoder->GetInputSource();
	mIsOpen			= mDecoder->IsOpen();

	if(mDecoder->IsOpen())
		SetupDecoder();
}

LoopableRegionDecoder::~LoopableRegionDecoder()
{
	if(IsOpen())
		Close();

	// Just set our references to nullptr, as mDecoder actually owns the objects and will delete them
	mInputSource	= nullptr;
	mChannelLayout	= nullptr;

	if(mDecoder)
		delete mDecoder, mDecoder = nullptr;
}

bool LoopableRegionDecoder::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.LoopableRegion", "Open() called on an AudioDecoder that is already open");		
		return true;
	}
	
	if(!mDecoder->IsOpen() && !mDecoder->Open(error))
		return false;

	if(!SetupDecoder(false)) {
		mDecoder->Close(error);
		return false;
	}

	mIsOpen = true;
	return true;
}

bool LoopableRegionDecoder::Close(CFErrorRef *error)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.AudioDecoder.LoopableRegion", "Close() called on an AudioDecoder that hasn't been opened");
		return true;
	}
	
	if(!mDecoder->Close(error))
		return false;

	mIsOpen = false;
	return true;
}

bool LoopableRegionDecoder::Reset()
{
	if(!IsOpen())
		return false;

	mFramesReadInCurrentPass	= 0;
	mTotalFramesRead			= 0;
	mCompletedPasses			= 0;

	return (mStartingFrame == mDecoder->SeekToFrame(mStartingFrame));
}

#pragma mark Functionality

SInt64 LoopableRegionDecoder::SeekToFrame(SInt64 frame)
{
	if(!IsOpen() || 0 > frame || frame >= GetTotalFrames())
		return -1;
	
	mCompletedPasses			= static_cast<UInt32>(frame / mFrameCount);
	mFramesReadInCurrentPass	= static_cast<UInt32>(frame % mFrameCount);
	mTotalFramesRead			= frame;
	
	mDecoder->SeekToFrame(mStartingFrame + mFramesReadInCurrentPass);
	
	return GetCurrentFrame();
}

UInt32 LoopableRegionDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(!IsOpen() || nullptr == bufferList || 0 == frameCount)
		return 0;
	
	// If the repeat count is N then (N + 1) passes must be completed to read all the frames
	if((1 + mRepeatCount) == mCompletedPasses)
		return 0;

	// Allocate an alias to the buffer list, which will contain pointers to the current write position in the output buffer
	AudioBufferList *bufferListAlias = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * bufferList->mNumberBuffers)));
	
	if(nullptr == bufferListAlias) {
		LOGGER_ERR("org.sbooth.AudioEngine.AudioDecoder.LoopableRegion", "Unable to allocate memory");
		return 0;
	}	

	UInt32 initialBufferCapacityBytes = bufferList->mBuffers[0].mDataByteSize;
	bufferListAlias->mNumberBuffers = bufferList->mNumberBuffers;

	// Initially the buffer list alias points to the beginning and contains no data
	for(UInt32 i = 0; i < bufferListAlias->mNumberBuffers; ++i) {
		bufferListAlias->mBuffers[i].mData				= bufferList->mBuffers[i].mData;
		bufferListAlias->mBuffers[i].mDataByteSize		= bufferList->mBuffers[i].mDataByteSize;
		bufferListAlias->mBuffers[i].mNumberChannels	= bufferList->mBuffers[i].mNumberChannels;

		bufferList->mBuffers[i].mDataByteSize			= 0;
	}
	
	UInt32 framesRemaining = frameCount;
	UInt32 totalFramesRead = 0;
	
	while(0 < framesRemaining) {
		UInt32 framesRemainingInCurrentPass	= static_cast<UInt32>(mStartingFrame + mFrameCount - mDecoder->GetCurrentFrame());
		UInt32 framesToRead					= std::min(framesRemaining, framesRemainingInCurrentPass);
		
		// Nothing left to read
		if(0 == framesToRead)
			break;

		UInt32 framesRead = mDecoder->ReadAudio(bufferListAlias, framesToRead);
		
		// A read error occurred
		if(0 == framesRead)
			break;

		// Advance the write pointers and update the capacity
		for(UInt32 i = 0; i < bufferListAlias->mNumberBuffers; ++i) {
			int8_t *buf									= static_cast<int8_t *>(bufferListAlias->mBuffers[i].mData);
			bufferListAlias->mBuffers[i].mData			= static_cast<void *>(buf + (framesRead * mFormat.mBytesPerFrame));

			bufferList->mBuffers[i].mDataByteSize		+= bufferListAlias->mBuffers[i].mDataByteSize;
			
			bufferListAlias->mBuffers[i].mDataByteSize	= initialBufferCapacityBytes - bufferList->mBuffers[i].mDataByteSize;
		}
		
		// Housekeeping
		mFramesReadInCurrentPass	+= framesRead;
		mTotalFramesRead			+= framesRead;
		
		totalFramesRead				+= framesRead;
		framesRemaining				-= framesRead;

		// If this pass is finished, seek to the beginning of the region in preparation for the next read
		if(mFrameCount == mFramesReadInCurrentPass) {
			++mCompletedPasses;
			mFramesReadInCurrentPass = 0;
			
			// Only seek to the beginning of the region if more passes remain
			if(mRepeatCount >= mCompletedPasses)
				mDecoder->SeekToFrame(mStartingFrame);
		}
	}
	
	free(bufferListAlias), bufferListAlias = nullptr;
	
	return totalFramesRead;
}

bool LoopableRegionDecoder::SetupDecoder(bool forceReset)
{
	assert(mDecoder);
	assert(mDecoder->IsOpen());

	mFormat			= mDecoder->GetFormat();
	mChannelLayout	= mDecoder->GetChannelLayout();
	mSourceFormat	= mDecoder->GetSourceFormat();
	
	if(0 == mFrameCount)
		mFrameCount = static_cast<UInt32>(mDecoder->GetTotalFrames() - mStartingFrame);
	
	if(forceReset || 0 != mStartingFrame)
		return Reset();

	return true;
}
