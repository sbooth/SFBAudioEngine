/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
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
#include <stdexcept>

#include "LoopableRegionDecoder.h"
#include "Logger.h"

#pragma mark Factory Methods

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForURLRegion(CFURLRef url, SInt64 startingFrame, CFErrorRef *error)
{
	return CreateForInputSourceRegion(InputSource::CreateInputSourceForURL(url, 0, error), startingFrame, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error)
{
	return CreateForInputSourceRegion(InputSource::CreateInputSourceForURL(url, 0, error), startingFrame, frameCount, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error)
{
	return CreateForInputSourceRegion(InputSource::CreateInputSourceForURL(url, 0, error), startingFrame, frameCount, repeatCount, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForInputSourceRegion(InputSource::unique_ptr inputSource, SInt64 startingFrame, CFErrorRef *error)
{
	if(!inputSource)
		return nullptr;

	return CreateForDecoderRegion(Decoder::CreateForInputSource(std::move(inputSource), error), startingFrame, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForInputSourceRegion(InputSource::unique_ptr inputSource, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error)
{
	if(!inputSource)
		return nullptr;

	return CreateForDecoderRegion(Decoder::CreateForInputSource(std::move(inputSource), error), startingFrame, frameCount, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForInputSourceRegion(InputSource::unique_ptr inputSource, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error)
{
	if(!inputSource)
		return nullptr;

	return CreateForDecoderRegion(Decoder::CreateForInputSource(std::move(inputSource), error), startingFrame, frameCount, repeatCount, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForDecoderRegion(Decoder::unique_ptr decoder, SInt64 startingFrame, CFErrorRef */*error*/)
{
	if(!decoder)
		return nullptr;

	return unique_ptr(new LoopableRegionDecoder(std::move(decoder), startingFrame));
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForDecoderRegion(Decoder::unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount, CFErrorRef */*error*/)
{
	if(!decoder)
		return nullptr;

	return unique_ptr(new LoopableRegionDecoder(std::move(decoder), startingFrame, frameCount));
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForDecoderRegion(Decoder::unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *)
{
	if(!decoder)
		return nullptr;

	return unique_ptr(new LoopableRegionDecoder(std::move(decoder), startingFrame, frameCount, repeatCount));
}

SFB::Audio::LoopableRegionDecoder::LoopableRegionDecoder(Decoder::unique_ptr decoder, SInt64 startingFrame)
	: mDecoder(std::move(decoder)), mStartingFrame(startingFrame), mFrameCount(0), mRepeatCount(0), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	if(!mDecoder)
		throw std::runtime_error("mDecoder may not be nullptr");
}

SFB::Audio::LoopableRegionDecoder::LoopableRegionDecoder(Decoder::unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount)
	: mDecoder(std::move(decoder)), mStartingFrame(startingFrame), mFrameCount(frameCount), mRepeatCount(0), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	if(!mDecoder)
		throw std::runtime_error("mDecoder may not be nullptr");
}

SFB::Audio::LoopableRegionDecoder::LoopableRegionDecoder(Decoder::unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount)
	: mDecoder(std::move(decoder)), mStartingFrame(startingFrame), mFrameCount(frameCount), mRepeatCount(repeatCount), mFramesReadInCurrentPass(0), mTotalFramesRead(0), mCompletedPasses(0)
{
	if(!mDecoder)
		throw std::runtime_error("mDecoder may not be nullptr");
}

bool SFB::Audio::LoopableRegionDecoder::_Open(CFErrorRef *error)
{
	if(!mDecoder->IsOpen() && !mDecoder->Open(error))
		return false;

	if(!mDecoder->SupportsSeeking() || !SetupDecoder(false)) {
		mDecoder->Close(error);
		return false;
	}

	return true;
}

bool SFB::Audio::LoopableRegionDecoder::_Close(CFErrorRef *error)
{
	if(!mDecoder->Close(error))
		return false;

	return true;
}

SFB::CFString SFB::Audio::LoopableRegionDecoder::_GetSourceFormatDescription() const
{
	return mDecoder->CreateSourceFormatDescription();
}

#pragma mark Functionality

UInt32 SFB::Audio::LoopableRegionDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	// If the repeat count is N then (N + 1) passes must be completed to read all the frames
	if((1 + mRepeatCount) == mCompletedPasses) {
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
			bufferList->mBuffers[bufferIndex].mDataByteSize = 0;
		return 0;
	}

	// Allocate an alias to the buffer list, which will contain pointers to the current write position in the output buffer
	AudioBufferList *bufferListAlias = (AudioBufferList *)alloca(offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * bufferList->mNumberBuffers));

	if(nullptr == bufferListAlias) {
		LOGGER_ERR("org.sbooth.AudioEngine.Decoder.LoopableRegion", "Unable to allocate memory");
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
		UInt32 framesRemainingInCurrentPass	= (UInt32)(mStartingFrame + mFrameCount - mDecoder->GetCurrentFrame());
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
			int8_t *buf									= (int8_t *)bufferListAlias->mBuffers[i].mData;
			bufferListAlias->mBuffers[i].mData			= (void *)(buf + (framesRead * mFormat.mBytesPerFrame));

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
		
	return totalFramesRead;
}

SInt64 SFB::Audio::LoopableRegionDecoder::_SeekToFrame(SInt64 frame)
{
	mCompletedPasses			= (UInt32)(frame / mFrameCount);
	mFramesReadInCurrentPass	= (UInt32)(frame % mFrameCount);
	mTotalFramesRead			= frame;

	mDecoder->SeekToFrame(mStartingFrame + mFramesReadInCurrentPass);

	return _GetCurrentFrame();
}

bool SFB::Audio::LoopableRegionDecoder::Reset()
{
	mFramesReadInCurrentPass	= 0;
	mTotalFramesRead			= 0;
	mCompletedPasses			= 0;

	return (mStartingFrame == mDecoder->SeekToFrame(mStartingFrame));
}

bool SFB::Audio::LoopableRegionDecoder::SetupDecoder(bool forceReset)
{
	mFormat			= mDecoder->GetFormat();
	mChannelLayout	= mDecoder->GetChannelLayout();
	mSourceFormat	= mDecoder->GetSourceFormat();
	
	if(0 == mFrameCount)
		mFrameCount = (UInt32)(mDecoder->GetTotalFrames() - mStartingFrame);
	
	if(forceReset || 0 != mStartingFrame)
		return Reset();

	return true;
}
