/*
 *  Copyright (C) 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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
#include "AllocateABL.h"
#include "DeallocateABL.h"

DecoderStateData::DecoderStateData()
	: mDecoder(nullptr), mBufferList(nullptr), mBufferCapacityFrames(0), mTimeStamp(0), mTotalFrames(0), mFramesRendered(0), mFrameToSeek(-1), mFlags(0)
{}

DecoderStateData::DecoderStateData(AudioDecoder *decoder)
	: DecoderStateData()
{
	assert(nullptr != decoder);
	mDecoder = decoder;
	
	// NB: The decoder may return an estimate of the total frames
	mTotalFrames = mDecoder->GetTotalFrames();
}
	
DecoderStateData::~DecoderStateData()
{
	// Delete the decoder
	if(mDecoder)
		delete mDecoder, mDecoder = nullptr;

	DeallocateBufferList();
}

void DecoderStateData::AllocateBufferList(UInt32 capacityFrames)
{
	DeallocateBufferList();

	mBufferCapacityFrames = capacityFrames;
	mBufferList = AllocateABL(mDecoder->GetFormat(), mBufferCapacityFrames);
}

void DecoderStateData::DeallocateBufferList()
{
	if(mBufferList) {
		mBufferCapacityFrames = 0;
		mBufferList = DeallocateABL(mBufferList);
	}
}

void DecoderStateData::ResetBufferList()
{
	AudioStreamBasicDescription formatDescription = mDecoder->GetFormat();
		
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = mBufferCapacityFrames * formatDescription.mBytesPerFrame;
}

UInt32 DecoderStateData::ReadAudio(UInt32 frameCount)
{
	if(nullptr == mDecoder)
		return 0;

	ResetBufferList();

	return mDecoder->ReadAudio(mBufferList, frameCount);
}
