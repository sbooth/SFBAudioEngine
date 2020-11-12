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
	 * @param dstOffset The byte offset in \c buffers to begin writing
	 * @param bufferList The source buffers
	 * @param srcOffset The byte offset in \c bufferList to begin reading
	 * @param byteCount The number of bytes per non-interleaved buffer to read and write
	 */
	inline void StoreABL(uint8_t **buffers, size_t dstOffset, const AudioBufferList *bufferList, size_t srcOffset, size_t byteCount)
	{
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
			memcpy(buffers[bufferIndex] + dstOffset, (uint8_t *)bufferList->mBuffers[bufferIndex].mData + srcOffset, byteCount);
	}

	/*!
	 * Copy non-interleaved audio from \c buffers to \c bufferList
	 * @param bufferList The destination buffers
	 * @param dstOffset The byte offset in \c bufferList to begin writing
	 * @param buffers The source buffers
	 * @param srcOffset The byte offset in \c bufferList to begin reading
	 * @param byteCount The number of bytes per non-interleaved buffer to read and write
	 */
	inline void FetchABL(AudioBufferList *bufferList, size_t dstOffset, const uint8_t **buffers, size_t srcOffset, size_t byteCount)
	{
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
			memcpy((uint8_t *)bufferList->mBuffers[bufferIndex].mData + dstOffset, buffers[bufferIndex] + srcOffset, byteCount);
	}

	/*!
	 * Return the smallest power of two value greater than \c x
	 * @param x A value in the range [2..2147483648]
	 * @return The smallest power of two greater than \c x
	 */
	inline constexpr uint32_t NextPowerOfTwo(uint32_t x)
	{
		assert(x > 1);
		assert(x <= ((UINT32_MAX / 2) + 1));
		return (uint32_t)1 << (32 - __builtin_clz(x - 1));
	}

}

#pragma mark Creation and Destruction

SFB::Audio::RingBuffer::RingBuffer()
	: mBuffers(nullptr), mCapacityFrames(0), mCapacityFramesMask(0), mWritePointer(0), mReadPointer(0)
{
	assert(mWritePointer.is_lock_free());
}

SFB::Audio::RingBuffer::~RingBuffer()
{
	Deallocate();
}

#pragma mark Buffer Management

bool SFB::Audio::RingBuffer::Allocate(const Format& format, size_t capacityFrames)
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
}

size_t SFB::Audio::RingBuffer::GetFramesAvailableToRead() const
{
	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	if(writePointer > readPointer)
		return writePointer - readPointer;
	else
		return (writePointer - readPointer + mCapacityFrames) & mCapacityFramesMask;
}

size_t SFB::Audio::RingBuffer::GetFramesAvailableToWrite() const
{
	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	if(writePointer > readPointer)
		return ((readPointer - writePointer + mCapacityFrames) & mCapacityFramesMask) - 1;
	else if(writePointer < readPointer)
		return (readPointer - writePointer) - 1;
	else
		return mCapacityFrames - 1;
}

size_t SFB::Audio::RingBuffer::Read(AudioBufferList *bufferList, size_t frameCount)
{
	if(nullptr == bufferList || 0 == frameCount)
		return 0;

	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	size_t framesAvailable;
	if(writePointer > readPointer)
		framesAvailable = writePointer - readPointer;
	else
		framesAvailable = (writePointer - readPointer + mCapacityFrames) & mCapacityFramesMask;

	if(0 == framesAvailable)
		return 0;

	size_t framesToRead = std::min(framesAvailable, frameCount);
	if(readPointer + framesToRead > mCapacityFrames) {
		auto framesAfterReadPointer = mCapacityFrames - readPointer;
		FetchABL(bufferList, 0, (const uint8_t **)mBuffers, mFormat.FrameCountToByteCount(readPointer), mFormat.FrameCountToByteCount(framesAfterReadPointer));
		FetchABL(bufferList, mFormat.FrameCountToByteCount(framesAfterReadPointer), (const uint8_t **)mBuffers, 0, mFormat.FrameCountToByteCount(framesToRead - framesAfterReadPointer));
	}
	else
		FetchABL(bufferList, 0, (const uint8_t **)mBuffers, mFormat.FrameCountToByteCount(readPointer), mFormat.FrameCountToByteCount(framesToRead));

	mReadPointer.store((readPointer + framesToRead) & mCapacityFramesMask, std::memory_order_release);

	// Set the ABL buffer sizes
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		bufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)mFormat.FrameCountToByteCount(framesToRead);

	return framesToRead;
}

size_t SFB::Audio::RingBuffer::Write(const AudioBufferList *bufferList, size_t frameCount)
{
	if(nullptr == bufferList || 0 == frameCount)
		return 0;

	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	size_t framesAvailable;
	if(writePointer > readPointer)
		framesAvailable = ((readPointer - writePointer + mCapacityFrames) & mCapacityFramesMask) - 1;
	else if(writePointer < readPointer)
		framesAvailable = (readPointer - writePointer) - 1;
	else
		framesAvailable = mCapacityFrames - 1;

	if(0 == framesAvailable)
		return 0;

	size_t framesToWrite = std::min(framesAvailable, frameCount);
	if(writePointer + framesToWrite > mCapacityFrames) {
		auto framesAfterWritePointer = mCapacityFrames - writePointer;
		StoreABL(mBuffers, mFormat.FrameCountToByteCount(writePointer), bufferList, 0, mFormat.FrameCountToByteCount(framesAfterWritePointer));
		StoreABL(mBuffers, 0, bufferList, mFormat.FrameCountToByteCount(framesAfterWritePointer), mFormat.FrameCountToByteCount(framesToWrite - framesAfterWritePointer));
	}
	else
		StoreABL(mBuffers, mFormat.FrameCountToByteCount(writePointer), bufferList, 0, mFormat.FrameCountToByteCount(framesToWrite));

	mWritePointer.store((writePointer + framesToWrite) & mCapacityFramesMask, std::memory_order_release);

	return framesToWrite;
}
