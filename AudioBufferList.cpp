/*
 *  Copyright (C) 2013, 2014 Stephen F. Booth <me@sbooth.org>
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

#include <cstdlib>
#include <stdexcept>

#include "AudioBufferList.h"

SFB::Audio::BufferList::BufferList()
	: mBufferList(nullptr, nullptr), mCapacityFrames(0)
{}

SFB::Audio::BufferList::BufferList(const AudioFormat& format, UInt32 capacityFrames)
	: BufferList()
{
	if(!Allocate(format, capacityFrames))
		throw std::bad_alloc();
}

bool SFB::Audio::BufferList::Allocate(const AudioFormat& format, UInt32 capacityFrames)
{
	if(mBufferList)
		Deallocate();

	UInt32 numBuffers = format.IsInterleaved() ? 1 : format.mChannelsPerFrame;
	UInt32 channelsPerBuffer = format.IsInterleaved() ? format.mChannelsPerFrame : 1;

	void *allocation = calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * numBuffers));
	if(nullptr == allocation)
		return false;

	// Use a custom deleter to ensure all allocations are freed
	mBufferList = std::unique_ptr<AudioBufferList, void (*)(AudioBufferList *)>((AudioBufferList *)allocation, [](AudioBufferList *bufferList) {
		if(nullptr != bufferList) {
			for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
				if(nullptr != bufferList->mBuffers[bufferIndex].mData)
					free(bufferList->mBuffers[bufferIndex].mData);
			}

			free(bufferList);
		}
	});

	mBufferList->mNumberBuffers = numBuffers;

	for(UInt32 bufferIndex = 0; bufferIndex < mBufferList->mNumberBuffers; ++bufferIndex) {
		// If the allocation fails cleanup will be handled by the unique_ptr's deleter
		void *data = calloc(1, format.FrameCountToByteCount(capacityFrames));
		if(nullptr == data)
			return false;

		mBufferList->mBuffers[bufferIndex].mData = data;
		mBufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)format.FrameCountToByteCount(capacityFrames);
		mBufferList->mBuffers[bufferIndex].mNumberChannels = channelsPerBuffer;
	}

	mFormat = format;
	mCapacityFrames = capacityFrames;

	return true;
}

bool SFB::Audio::BufferList::Deallocate()
{
	if(!mBufferList)
		return false;

	mCapacityFrames = 0;
	mFormat = {};

	mBufferList.reset();

	return true;
}

bool SFB::Audio::BufferList::Reset()
{
	if(!mBufferList)
		return false;

	for(UInt32 bufferIndex = 0; bufferIndex < mBufferList->mNumberBuffers; ++bufferIndex)
		mBufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)mFormat.FrameCountToByteCount(mCapacityFrames);

	return true;
}
