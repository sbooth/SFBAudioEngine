/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>
#include <cstdlib>

#include "AudioRingBuffer.h"

namespace {

	/*!
	 * Copy non-interleaved audio from \c bufferList to \c buffers
	 * @param buffers The destination buffers
	 * @param destOffset The byte offset in \c buffers to begin writing
	 * @param bufferList The source buffers
	 * @param srcOffset The byte offset in \c bufferList to begin reading
	 * @param byteCount The number of bytes per non-interleaved buffer to read and write
	 */
	inline void StoreABL(uint8_t **buffers, size_t destOffset, const AudioBufferList *bufferList, size_t srcOffset, size_t byteCount)
	{
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
			memcpy(buffers[bufferIndex] + destOffset, (uint8_t *)bufferList->mBuffers[bufferIndex].mData + srcOffset, byteCount);
	}

	/*!
	 * Copy non-interleaved audio from \c buffers to \c bufferList
	 * @param bufferList The destination buffers
	 * @param destOffset The byte offset in \c bufferList to begin writing
	 * @param buffers The source buffers
	 * @param srcOffset The byte offset in \c bufferList to begin reading
	 * @param byteCount The number of bytes per non-interleaved buffer to read and write
	 */
	inline void FetchABL(AudioBufferList *bufferList, size_t destOffset, const uint8_t **buffers, size_t srcOffset, size_t byteCount)
	{
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
			memcpy((uint8_t *)bufferList->mBuffers[bufferIndex].mData + destOffset, buffers[bufferIndex] + srcOffset, byteCount);
	}

	/*!
	 * Return the smallest power of two value greater than \c x
	 * @param x A value in the range [2..2147483648]
	 * @return The smallest power of two greater than \c x
	 *
	 */
	__attribute__ ((const)) inline uint32_t NextPowerOfTwo(uint32_t x)
	{
#if 0
		assert(x > 1);
		assert(x <= ((UINT32_MAX / 2) + 1));
#endif

		return 1 << (32 - __builtin_clz(x - 1));
	}

}

#pragma mark Creation and Destruction

SFB::Audio::RingBuffer::RingBuffer()
	: mBuffers(nullptr), mCapacityFrames(0), mCapacityFramesMask(0), mWritePointer(0), mReadPointer(0)
{}

SFB::Audio::RingBuffer::~RingBuffer()
{
	Deallocate();
}

#pragma mark Buffer Management

bool SFB::Audio::RingBuffer::Allocate(const AudioFormat& format, size_t capacityFrames)
{
	// Only non-interleaved formats are supported
	if(format.IsInterleaved())
		return false;

	Deallocate();

	// Round up to the next power of two
	capacityFrames = NextPowerOfTwo((uint32_t)capacityFrames);

	mFormat = format;

	mCapacityFrames = capacityFrames;
	mCapacityFramesMask = capacityFrames - 1;

	size_t capacityBytes = format.FrameCountToByteCount(capacityFrames);

	// One memory allocation holds everything- first the pointers followed by the deinterleaved channels
	size_t allocationSize = (capacityBytes + sizeof(uint8_t *)) * format.mChannelsPerFrame;
	uint8_t *memoryChunk = (uint8_t *)malloc(allocationSize);
	if(nullptr == memoryChunk)
		return false;

	// Zero the entire allocation
	memset(memoryChunk, 0, allocationSize);

	// Assign the pointers and channel buffers
	mBuffers = (uint8_t **)memoryChunk;
	memoryChunk += format.mChannelsPerFrame * sizeof(uint8_t *);
	for(UInt32 i = 0; i < format.mChannelsPerFrame; ++i) {
		mBuffers[i] = memoryChunk;
		memoryChunk += capacityBytes;
	}

	mReadPointer = 0;
	mWritePointer = 0;

	return true;
}

void SFB::Audio::RingBuffer::Deallocate()
{
	if(mBuffers) {
		free(mBuffers);
		mBuffers = nullptr;
	}
}


void SFB::Audio::RingBuffer::Reset()
{
	mReadPointer = 0;
	mWritePointer = 0;

	for(UInt32 i = 0; i < mFormat.mChannelsPerFrame; ++i)
		memset(mBuffers[i], 0, mFormat.FrameCountToByteCount(mCapacityFrames));
}

size_t SFB::Audio::RingBuffer::GetFramesAvailableToRead() const
{
	size_t w = mWritePointer;
	size_t r = mReadPointer;

	if(w > r)
		return w - r;
	else
		return (w - r + mCapacityFrames) & mCapacityFramesMask;
}

size_t SFB::Audio::RingBuffer::GetFramesAvailableToWrite() const
{
	size_t w = mWritePointer;
	size_t r = mReadPointer;

	if(w > r)
		return ((r - w + mCapacityFrames) & mCapacityFramesMask) - 1;
	else if(w < r)
		return (r - w) - 1;
	else
		return mCapacityFrames - 1;
}

size_t SFB::Audio::RingBuffer::ReadAudio(AudioBufferList *bufferList, size_t frameCount)
{
	if(0 == frameCount)
		return 0;

	size_t framesAvailable = GetFramesAvailableToRead();
	if(0 == framesAvailable)
		return 0;

	size_t framesToRead = std::min(framesAvailable, frameCount);
	size_t cnt2 = mReadPointer + framesToRead;

	size_t n1, n2;
	if(cnt2 > mCapacityFrames) {
		n1 = mCapacityFrames - mReadPointer;
		n2 = cnt2 & mCapacityFramesMask;
	}
	else {
		n1 = framesToRead;
		n2 = 0;
	}

	FetchABL(bufferList, 0, (const uint8_t **)mBuffers, mFormat.FrameCountToByteCount(mReadPointer), mFormat.FrameCountToByteCount(n1));
	mReadPointer = (mReadPointer + n1) & mCapacityFramesMask;

	if(n2) {
		FetchABL(bufferList, mFormat.FrameCountToByteCount(n1), (const uint8_t **)mBuffers, mFormat.FrameCountToByteCount(mReadPointer), mFormat.FrameCountToByteCount(n2));
		mReadPointer = (mReadPointer + n2) & mCapacityFramesMask;
	}

	// Set the buffer sizes
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		bufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)mFormat.FrameCountToByteCount(framesToRead);

	return framesToRead;
}

size_t SFB::Audio::RingBuffer::WriteAudio(const AudioBufferList *bufferList, size_t frameCount)
{
	if(0 == frameCount)
		return 0;

	size_t framesAvailable = GetFramesAvailableToWrite();
	if(0 == framesAvailable)
		return 0;

	size_t framesToWrite = std::min(framesAvailable, frameCount);
	size_t cnt2 = mWritePointer + framesToWrite;

	size_t n1, n2;
	if(cnt2 > mCapacityFrames) {
		n1 = mCapacityFrames - mWritePointer;
		n2 = cnt2 & mCapacityFramesMask;
	}
	else {
		n1 = framesToWrite;
		n2 = 0;
	}

	StoreABL(mBuffers, mFormat.FrameCountToByteCount(mWritePointer), bufferList, 0, mFormat.FrameCountToByteCount(n1));
	mWritePointer = (mWritePointer + n1) & mCapacityFramesMask;

	if(n2) {
		StoreABL(mBuffers, mFormat.FrameCountToByteCount(mWritePointer), bufferList, mFormat.FrameCountToByteCount(n1), mFormat.FrameCountToByteCount(n2));
		mWritePointer = (mWritePointer + n2) & mCapacityFramesMask;
	}

	return framesToWrite;
}
