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

#include "LoopableRegionDecoder.h"
#include "AudioDecoder.h"

#include <algorithm>


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

#pragma mark Decoding

UInt32 LoopableRegionDecoder::ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	assert(NULL != bufferList);
	assert(0 < frameCount);
	
	// If the repeat count is N then (N + 1) passes must be completed to read all the frames
	if((1 + mRepeatCount) == mCompletedPasses)
		return 0;

	UInt32 framesRemaining = frameCount;
	UInt32 totalFramesRead = 0;
	
	while(0 < framesRemaining) {
		UInt32 framesRemainingInCurrentPass	= static_cast<UInt32>(mStartingFrame + mFrameCount - mDecoder->GetCurrentFrame());
		UInt32 framesToRead					= std::min(framesRemaining, framesRemainingInCurrentPass);
		
		// Nothing left to read
		if(0 == framesToRead)
			break;
		
		UInt32 framesRead = mDecoder->ReadAudio(bufferList, framesToRead);
		
		// A read error occurred
		if(0 == framesRead)
			break;

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
	
	return totalFramesRead;
}

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
