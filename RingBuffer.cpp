/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>
#include <atomic>
#include <cstdlib>

#include "RingBuffer.h"

namespace {

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

SFB::RingBuffer::RingBuffer()
	: mBuffer(nullptr), mCapacityBytes(0), mCapacityBytesMask(0), mWritePosition(0), mReadPosition(0)
{}

SFB::RingBuffer::~RingBuffer()
{
	Deallocate();
}

#pragma mark Buffer Management

bool SFB::RingBuffer::Allocate(size_t capacityBytes)
{
	Deallocate();

	// Round up to the next power of two
	capacityBytes = NextPowerOfTwo((uint32_t)capacityBytes);

	mCapacityBytes = capacityBytes;
	mCapacityBytesMask = capacityBytes - 1;

	try {
		mBuffer = new uint8_t [mCapacityBytes];
	}

	catch(const std::exception& e) {
		return false;
	}

	mReadPosition = 0;
	mWritePosition = 0;

	return true;
}

void SFB::RingBuffer::Deallocate()
{
	if(mBuffer) {
		delete [] mBuffer;
		mBuffer = nullptr;
	}
}


void SFB::RingBuffer::Reset()
{
	mReadPosition = 0;
	mWritePosition = 0;
}

size_t SFB::RingBuffer::GetBytesAvailableToRead() const
{
	auto w = mWritePosition;
	auto r = mReadPosition;

	if(w > r)
		return w - r;
	else
		return (w - r + mCapacityBytes) & mCapacityBytesMask;
}

size_t SFB::RingBuffer::GetBytesAvailableToWrite() const
{
	auto w = mWritePosition;
	auto r = mReadPosition;

	if(w > r)
		return ((r - w + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(w < r)
		return (r - w) - 1;
	else
		return mCapacityBytes - 1;
}

size_t SFB::RingBuffer::Read(void *destinationBuffer, size_t byteCount)
{
	if(nullptr == destinationBuffer || 0 == byteCount)
		return 0;

	auto rv = GetReadVector();

	auto bytesAvailable = rv.first.mBufferCapacity + rv.second.mBufferCapacity;
	auto bytesToRead = std::min(bytesAvailable, byteCount);
	if(bytesToRead > rv.first.mBufferCapacity) {
		memcpy(destinationBuffer, rv.first.mBuffer, rv.first.mBufferCapacity);
		memcpy((uint8_t *)destinationBuffer + rv.first.mBufferCapacity, rv.second.mBuffer, bytesToRead - rv.first.mBufferCapacity);
	}
	else
		memcpy(destinationBuffer, rv.first.mBuffer, bytesToRead);

	AdvanceReadPosition(bytesToRead);

	return bytesToRead;
}

size_t SFB::RingBuffer::Peek(void *destinationBuffer, size_t byteCount) const
{
	if(nullptr == destinationBuffer || 0 == byteCount)
		return 0;

	auto rv = GetReadVector();

	auto bytesAvailable = rv.first.mBufferCapacity + rv.second.mBufferCapacity;
	auto bytesToRead = std::min(bytesAvailable, byteCount);
	if(bytesToRead > rv.first.mBufferCapacity) {
		memcpy(destinationBuffer, rv.first.mBuffer, rv.first.mBufferCapacity);
		memcpy((uint8_t *)destinationBuffer + rv.first.mBufferCapacity, rv.second.mBuffer, bytesToRead - rv.first.mBufferCapacity);
	}
	else
		memcpy(destinationBuffer, rv.first.mBuffer, bytesToRead);

	return bytesToRead;
}

size_t SFB::RingBuffer::Write(const void *sourceBuffer, size_t byteCount)
{
	if(nullptr == sourceBuffer || 0 == byteCount)
		return 0;

	auto wv = GetWriteVector();

	auto bytesAvailable = wv.first.mBufferCapacity + wv.second.mBufferCapacity;
	auto bytesToWrite = std::min(bytesAvailable, byteCount);
	if(bytesToWrite > wv.first.mBufferCapacity) {
		memcpy(wv.first.mBuffer, sourceBuffer, wv.first.mBufferCapacity);
		memcpy(wv.second.mBuffer, (uint8_t *)sourceBuffer + wv.first.mBufferCapacity, bytesToWrite - wv.first.mBufferCapacity);
	}
	else
		memcpy(wv.first.mBuffer, sourceBuffer, bytesToWrite);

	AdvanceWritePosition(bytesToWrite);

	return bytesToWrite;
}

void SFB::RingBuffer::AdvanceReadPosition(size_t byteCount)
{
	std::atomic_thread_fence(std::memory_order_acq_rel);
	mReadPosition = (mReadPosition + byteCount) & mCapacityBytesMask;
}

void SFB::RingBuffer::AdvanceWritePosition(size_t byteCount)
{
	std::atomic_thread_fence(std::memory_order_release);
	mWritePosition = (mWritePosition + byteCount) & mCapacityBytesMask;;
}

SFB::RingBuffer::BufferPair SFB::RingBuffer::GetReadVector() const
{
	auto w = mWritePosition;
	auto r = mReadPosition;

	size_t free_cnt;
	if(w > r)
		free_cnt = w - r;
	else
		free_cnt = (w - r + mCapacityBytes) & mCapacityBytesMask;

	auto cnt2 = r + free_cnt;

	SFB::RingBuffer::BufferPair rv;
	if(cnt2 > mCapacityBytes)
		rv = { { mBuffer + r, mCapacityBytes - r }, { mBuffer, cnt2 & mCapacityBytes } };
	else
		rv = { { mBuffer + r, free_cnt }, {} };

	std::atomic_thread_fence(std::memory_order_acquire);
	return rv;
}

SFB::RingBuffer::BufferPair SFB::RingBuffer::GetWriteVector() const
{
	auto w = mWritePosition;
	auto r = mReadPosition;

	size_t free_cnt;
	if(w > r)
		free_cnt = ((r - w + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(w < r)
		free_cnt = (r - w) - 1;
	else
		free_cnt = mCapacityBytes - 1;

	auto cnt2 = w + free_cnt;

	SFB::RingBuffer::BufferPair wv;
	if(cnt2 > mCapacityBytes)
		wv = { { mBuffer + w, mCapacityBytes - w }, { mBuffer, cnt2 & mCapacityBytes } };
	else
		wv = { { mBuffer + w, free_cnt }, {} };

	std::atomic_thread_fence(std::memory_order_acq_rel);
	return wv;
}
