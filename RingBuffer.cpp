/*
 *  Copyright (C) 2013 Stephen F. Booth <me@sbooth.org>
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

#include "RingBuffer.h"
#include "Logger.h"

#include <stdlib.h>
#include <algorithm>

#include <libkern/OSAtomic.h>

// ========================================

/**
 * return the smallest power of two value
 * greater than x
 *
 * Input range:  [2..2147483648]
 * Output range: [2..2147483648]
 *
 */
__attribute__ ((const)) static inline uint32_t p2(uint32_t x)
{
#if 0
    assert(x > 1);
    assert(x <= ((UINT32_MAX / 2) + 1));
#endif

    return 1 << (32 - __builtin_clz (x - 1));
}

// From http://graphics.stanford.edu/~seander/bithacks.html
inline static unsigned int NextPowerOfTwo(unsigned int v)
{
	v--;

	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;

	v++;

	return v;
}

inline static void ZeroRange(unsigned char **buffers, UInt32 bufferCount, UInt32 byteOffset, UInt32 byteCount)
{
	for(UInt32 bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
		memset(buffers[bufferIndex] + byteOffset, 0, byteCount);
}

inline static void ZeroABL(AudioBufferList *bufferList, UInt32 byteOffset, UInt32 byteCount)
{
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		memset((unsigned char *)bufferList->mBuffers[bufferIndex].mData + byteOffset, 0, byteCount);
}

inline static void StoreABL(unsigned char **buffers, UInt32 destOffset, const AudioBufferList *bufferList, UInt32 srcOffset, UInt32 byteCount)
{
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		memcpy(buffers[bufferIndex] + destOffset, (unsigned char *)bufferList->mBuffers[bufferIndex].mData + srcOffset, byteCount);
}

inline static void FetchABL(AudioBufferList *bufferList, UInt32 destOffset, const unsigned char **buffers, UInt32 srcOffset, UInt32 byteCount)
{
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		memcpy((unsigned char *)bufferList->mBuffers[bufferIndex].mData + destOffset, buffers[bufferIndex] + srcOffset, byteCount);
}

#pragma mark Creation and Destruction

RingBuffer::RingBuffer()
	: mBuffers(nullptr), mNumberChannels(0), mCapacityFrames(0), mCapacityBytes(0)
{}

RingBuffer::~RingBuffer()
{
	Deallocate();
}

bool RingBuffer::Allocate(const AudioStreamBasicDescription& format, UInt32 capacityFrames)
{
	return Allocate(format.mChannelsPerFrame, format.mBytesPerFrame, capacityFrames);
}

bool RingBuffer::Allocate(UInt32 channelCount, UInt32 bytesPerFrame, UInt32 capacityFrames)
{
	Deallocate();

	// Round up to the next power of two
	capacityFrames = NextPowerOfTwo(capacityFrames);
	
	mNumberChannels = channelCount;
	mBytesPerFrame = bytesPerFrame;
	mCapacityFrames = capacityFrames;
	mCapacityFramesMask = capacityFrames - 1;
	mCapacityBytes = bytesPerFrame * capacityFrames;

	// One memory allocation holds everything- first the pointers followed by the deinterleaved channels
	UInt32 allocationSize = (mCapacityBytes + sizeof(unsigned char *)) * channelCount;
	unsigned char *memoryChunk = (unsigned char *)malloc(allocationSize);
	if(nullptr == memoryChunk)
		return false;

	// Zero the entire allocation
	memset(memoryChunk, 0, allocationSize);

	// Assign the pointers and channel buffers
	mBuffers = (unsigned char **)memoryChunk;
	memoryChunk += channelCount * sizeof(unsigned char *);
	for(UInt32 i = 0; i < channelCount; ++i) {
		mBuffers[i] = memoryChunk;
		memoryChunk += mCapacityBytes;
	}

	// Zero the time bounds queue
	for(UInt32 i = 0; i < kGeneralRingTimeBoundsQueueSize; ++i) {
		mTimeBoundsQueue[i].mStartTime = 0;
		mTimeBoundsQueue[i].mEndTime = 0;
		mTimeBoundsQueue[i].mUpdateCounter = 0;
	}

	mTimeBoundsQueueCounter = 0;

	return true;
}

void RingBuffer::Deallocate()
{
	if(mBuffers)
		free(mBuffers), mBuffers = nullptr;

	mNumberChannels = 0;
	mCapacityBytes = 0;
	mCapacityFrames = 0;
}

bool RingBuffer::Store(const AudioBufferList *bufferList, UInt32 frameCount, SampleTime startWrite)
{
	if(0 == frameCount)
		return true;

	if(frameCount > mCapacityFrames) {
#if DEBUG
		LOGGER_ERR("org.sbooth.AudioEngine.RingBuffer", "Insufficient ring buffer capacity: caller attempted to store " << frameCount << " frames, maximum " << mCapacityFrames);
#endif
		return false;
	}

	SampleTime endWrite = startWrite + frameCount;
	
	// going backwards, throw everything out
	if(startWrite < EndTime())
		SetTimeBounds(startWrite, startWrite);
	else if(endWrite - StartTime() <= mCapacityFrames) {
		// the buffer has not yet wrapped and will not need to
	}
	else {
		// advance the start time past the region we are about to overwrite
		SampleTime newStart = endWrite - mCapacityFrames;	// one buffer of time behind where we're writing
		SampleTime newEnd = std::max(newStart, EndTime());
		SetTimeBounds(newStart, newEnd);
	}
	
	// write the new frames
	unsigned char **buffers = mBuffers;
	UInt32 channelCount = mNumberChannels;
	UInt32 offset0, offset1, byteCount;
	SampleTime curEnd = EndTime();
	
	if(startWrite > curEnd) {
		// we are skipping some samples, so zero the range we are skipping
		offset0 = FrameOffset(curEnd);
		offset1 = FrameOffset(startWrite);

		if(offset0 < offset1)
			ZeroRange(buffers, channelCount, offset0, offset1 - offset0);
		else {
			ZeroRange(buffers, channelCount, offset0, mCapacityBytes - offset0);
			ZeroRange(buffers, channelCount, 0, offset1);
		}

		offset0 = offset1;
	}
	else
		offset0 = FrameOffset(startWrite);

	offset1 = FrameOffset(endWrite);
	if(offset0 < offset1)
		StoreABL(buffers, offset0, bufferList, 0, offset1 - offset0);
	else {
		byteCount = mCapacityBytes - offset0;
		StoreABL(buffers, offset0, bufferList, 0, byteCount);
		StoreABL(buffers, 0, bufferList, byteCount, offset1);
	}
	
	// now update the end time
	SetTimeBounds(StartTime(), endWrite);
	
	return true;
}

bool RingBuffer::Fetch(AudioBufferList *bufferList, UInt32 frameCount, SampleTime startRead) const
{
	if(0 == frameCount)
		return true;

	SampleTime endRead = startRead + frameCount;

	SampleTime startRead0 = startRead;
	SampleTime endRead0 = endRead;
	SampleTime size;

	if(!ConstrainTimesToBounds(startRead, endRead))
		return false;

	size = endRead - startRead;

	// Don't perform out-of-bounds writes
	if((startRead - startRead0) > frameCount || (endRead0 - endRead) > frameCount || startRead == endRead) {
		ZeroABL(bufferList, 0, frameCount * mBytesPerFrame);
		return true;
	}

	UInt32 destStartOffset = (UInt32)(startRead - startRead0);
	if(destStartOffset > 0)
		ZeroABL(bufferList, 0, destStartOffset * mBytesPerFrame);

	UInt32 destEndSize = (UInt32)(endRead0 - endRead);
	if(destEndSize > 0)
		ZeroABL(bufferList, (UInt32)(destStartOffset + size), destEndSize * mBytesPerFrame);

	const unsigned char **buffers = (const unsigned char **)mBuffers;
	UInt32 offset0 = FrameOffset(startRead);
	UInt32 offset1 = FrameOffset(endRead);
	UInt32 byteCount;

	if(offset0 < offset1)
		FetchABL(bufferList, destStartOffset, buffers, offset0, byteCount = offset1 - offset0);
	else {
		byteCount = mCapacityBytes - offset0;
		FetchABL(bufferList, destStartOffset, buffers, offset0, byteCount);
		FetchABL(bufferList, destStartOffset + byteCount, buffers, 0, offset1);
		byteCount += offset1;
	}

	// Set the buffer sizes
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		bufferList->mBuffers[bufferIndex].mDataByteSize = byteCount;

	return true;
}

// Get the range of timestamps contained in the buffer
bool RingBuffer::GetTimeBounds(SampleTime& startTime, SampleTime& endTime) const
{
	for(int i = 0; i < 8; ++i) {
		UInt32 currentCounter = mTimeBoundsQueueCounter;
		UInt32 currentIndex = currentCounter & kGeneralRingTimeBoundsQueueMask;

		const RingBuffer::TimeBounds *bounds = mTimeBoundsQueue + currentIndex;

		startTime = bounds->mStartTime;
		endTime = bounds->mEndTime;

		UInt32 counter = bounds->mUpdateCounter;

		if(counter == currentCounter)
			return true;
	}

#if DEBUG
	LOGGER_ERR("org.sbooth.AudioEngine.RingBuffer", "CPU overload: Unable to determine time bounds")
#endif

	return false;
}

#pragma mark Internals

// Set the range of timestamps contained in the buffer
void RingBuffer::SetTimeBounds(SampleTime startTime, SampleTime endTime)
{
	UInt32 nextCounter = mTimeBoundsQueueCounter + 1;
	UInt32 nextIndex = nextCounter & kGeneralRingTimeBoundsQueueMask;
	
	mTimeBoundsQueue[nextIndex].mStartTime = startTime;
	mTimeBoundsQueue[nextIndex].mEndTime = endTime;
	mTimeBoundsQueue[nextIndex].mUpdateCounter = nextCounter;

	OSAtomicIncrement32Barrier((int32_t *)&mTimeBoundsQueueCounter);
}

// Constrain startRead and endRead to valid timestamps in the buffer
bool RingBuffer::ConstrainTimesToBounds(SampleTime& startRead, SampleTime& endRead) const
{
	SampleTime startTime, endTime;

	if(!GetTimeBounds(startTime, endTime))
		return false;

	startRead = std::max(startRead, startTime);
	endRead = std::min(endRead, endTime);
	endRead = std::max(endRead, startRead);

	return true;
}
