/*
 *  Copyright (C) 2006, 2007, 2008, 2009 Stephen F. Booth <me@sbooth.org>
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
#include "AudioEngineDefines.h"
#include "AudioDecoder.h"


LoopableRegionDecoder::LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame)
	: mDecoder(decoder), mStartingFrame(startingFrame), mFrameCount(0), mRepeatCount(0), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	assert(NULL != decoder);
	assert(decoder->SupportsSeeking());
	
	mFormat			= mDecoder->GetFormat();
	mChannelLayout	= mDecoder->GetChannelLayout();
	mSourceFormat	= mDecoder->GetSourceFormat();
	
	mFrameCount		= static_cast<UInt32>(mDecoder->GetTotalFrames() - mStartingFrame);
	
	if(0 != mStartingFrame)
		Reset();
}

LoopableRegionDecoder::LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount)
	: mDecoder(decoder), mStartingFrame(startingFrame), mFrameCount(frameCount), mRepeatCount(0), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	assert(NULL != decoder);
	assert(decoder->SupportsSeeking());

	mFormat			= mDecoder->GetFormat();
	mChannelLayout	= mDecoder->GetChannelLayout();
	mSourceFormat	= mDecoder->GetSourceFormat();
	
	if(0 != mStartingFrame)
		Reset();
}

LoopableRegionDecoder::LoopableRegionDecoder(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount)
	: mDecoder(decoder), mStartingFrame(startingFrame), mFrameCount(frameCount), mRepeatCount(repeatCount), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	assert(NULL != decoder);
	assert(decoder->SupportsSeeking());
	
	mFormat			= mDecoder->GetFormat();
	mChannelLayout	= mDecoder->GetChannelLayout();
	mSourceFormat	= mDecoder->GetSourceFormat();
	
	if(0 != mStartingFrame)
		Reset();
}

LoopableRegionDecoder::~LoopableRegionDecoder()
{
	if(mDecoder)
		delete mDecoder, mDecoder = NULL;
}

void LoopableRegionDecoder::Reset()
{
	mDecoder->SeekToFrame(mStartingFrame);
	
	mFramesReadInCurrentPass	= 0;
	mTotalFramesRead			= 0;
	mCompletedPasses			= 0;
}


#pragma mark Functionality


SInt64 LoopableRegionDecoder::SeekToFrame(SInt64 frame)
{
	assert(0 <= frame);
	assert(frame < GetTotalFrames());
	
	mCompletedPasses			= static_cast<UInt32>(frame / mFrameCount);
	mFramesReadInCurrentPass	= static_cast<UInt32>(frame % mFrameCount);
	mTotalFramesRead			= frame;
	
	mDecoder->SeekToFrame(mStartingFrame + mFramesReadInCurrentPass);
	
	return GetCurrentFrame();
}

UInt32 LoopableRegionDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(bufferList->mNumberBuffers == mFormat.mChannelsPerFrame);
	assert(0 < frameCount);
	
	// If the repeat count is N then (N + 1) passes must be completed to read all the frames
	if((1 + mRepeatCount) == mCompletedPasses)
		return 0;

	// Allocate an alias to the buffer list, which will contain pointers to the current write position in the output buffer
	AudioBufferList *bufferListAlias = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * mFormat.mChannelsPerFrame)));
	
	if(NULL == bufferListAlias) {
		ERR("Unable to allocate memory");
		return 0;
	}	

	UInt32 initialBufferCapacityBytes = bufferList->mBuffers[0].mDataByteSize;
	bufferListAlias->mNumberBuffers = mFormat.mChannelsPerFrame;

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
			float *buf									= static_cast<float *>(bufferListAlias->mBuffers[i].mData);
			bufferListAlias->mBuffers[i].mData			= static_cast<void *>(buf + framesRead);

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
	
	free(bufferListAlias), bufferListAlias = NULL;
	
	return totalFramesRead;
}
