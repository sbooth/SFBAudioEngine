/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
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

bool SFB::Audio::BufferList::Empty()
{
	if(!mBufferList)
		return false;

	for(UInt32 bufferIndex = 0; bufferIndex < mBufferList->mNumberBuffers; ++bufferIndex)
		mBufferList->mBuffers[bufferIndex].mDataByteSize = 0;

	return true;
}
