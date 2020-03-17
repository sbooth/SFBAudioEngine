/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>
#include <stdexcept>

#include <os/log.h>

#include "LoopableRegionDecoder.h"

#pragma mark Factory Methods

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForURLRegion(CFURLRef url, SInt64 startingFrame, CFErrorRef *error)
{
	return CreateForInputSourceRegion(InputSource::CreateForURL(url, 0, error), startingFrame, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error)
{
	return CreateForInputSourceRegion(InputSource::CreateForURL(url, 0, error), startingFrame, frameCount, error);
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::LoopableRegionDecoder::CreateForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error)
{
	return CreateForInputSourceRegion(InputSource::CreateForURL(url, 0, error), startingFrame, frameCount, repeatCount, error);
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
	return CFString(mDecoder->CreateSourceFormatDescription());
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
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");
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
