/*
 *  Copyright (C) 2009, 2010 Stephen F. Booth <me@sbooth.org>
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

#include "DecoderStateData.h"
#include "AudioDecoder.h"
//#include "AudioEngineDefines.h"

DecoderStateData::DecoderStateData()
	: mDecoder(NULL), mBufferList(NULL), mBufferCapacityFrames(0), mTimeStamp(0), mTotalFrames(0), mFramesRendered(0), mFrameToSeek(-1), mDecodingFinished(false), mRenderingFinished(false)
{}

DecoderStateData::DecoderStateData(AudioDecoder *decoder)
	: mDecoder(decoder), mBufferList(NULL), mBufferCapacityFrames(0), mTimeStamp(0), mFramesRendered(0), mFrameToSeek(-1), mDecodingFinished(false), mRenderingFinished(false)
{
	assert(NULL != decoder);
	
	// NB: The decoder may return an estimate of the total frames
	mTotalFrames = mDecoder->GetTotalFrames();
}
	
DecoderStateData::~DecoderStateData()
{
	// Delete the decoder
	if(NULL != mDecoder)
		delete mDecoder, mDecoder = NULL;

	DeallocateBufferList();
}

void DecoderStateData::AllocateBufferList(UInt32 capacityFrames)
{
	DeallocateBufferList();

	mBufferCapacityFrames = capacityFrames;

	AudioStreamBasicDescription formatDescription = mDecoder->GetFormat();
	
	UInt32 numBuffers = (kAudioFormatFlagIsNonInterleaved & formatDescription.mFormatFlags) ? formatDescription.mChannelsPerFrame : 1;
	UInt32 channelsPerBuffer = (kAudioFormatFlagIsNonInterleaved & formatDescription.mFormatFlags) ? 1 : formatDescription.mChannelsPerFrame;
	
	mBufferList = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * numBuffers)));
	
	mBufferList->mNumberBuffers = numBuffers;
	
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
		mBufferList->mBuffers[i].mData = static_cast<void *>(calloc(mBufferCapacityFrames, formatDescription.mBytesPerFrame));
		mBufferList->mBuffers[i].mDataByteSize = mBufferCapacityFrames * formatDescription.mBytesPerFrame;
		mBufferList->mBuffers[i].mNumberChannels = channelsPerBuffer;
	}
}

void DecoderStateData::DeallocateBufferList()
{
	if(NULL != mBufferList) {
		mBufferCapacityFrames = 0;

		for(UInt32 bufferIndex = 0; bufferIndex < mBufferList->mNumberBuffers; ++bufferIndex)
			free(mBufferList->mBuffers[bufferIndex].mData), mBufferList->mBuffers[bufferIndex].mData = NULL;
		
		free(mBufferList), mBufferList = NULL;
	}
}

void DecoderStateData::ResetBufferList()
{
	AudioStreamBasicDescription formatDescription = mDecoder->GetFormat();
		
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = mBufferCapacityFrames * formatDescription.mBytesPerFrame;
}
