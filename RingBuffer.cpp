/*
 * Copyright (c) 2014 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "RingBuffer.h"

#include <cstdlib>
#include <algorithm>

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
	: mBuffer(nullptr), mCapacityBytes(0), mCapacityBytesMask(0), mWritePointer(0), mReadPointer(0)
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

	mReadPointer = 0;
	mWritePointer = 0;

	return true;
}

void SFB::RingBuffer::Deallocate()
{
	if(mBuffer)
		delete [] mBuffer, mBuffer = nullptr;
}


void SFB::RingBuffer::Reset()
{
	mReadPointer = 0;
	mWritePointer = 0;
}

size_t SFB::RingBuffer::GetBytesAvailableToRead() const
{
	auto w = mWritePointer;
	auto r = mReadPointer;

	if(w > r)
		return w - r;
	else
		return (w - r + mCapacityBytes) & mCapacityBytesMask;
}

size_t SFB::RingBuffer::GetBytesAvailableToWrite() const
{
	auto w = mWritePointer;
	auto r = mReadPointer;

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

	auto bytesAvailable = GetBytesAvailableToRead();
	if(0 == bytesAvailable)
		return 0;

	auto bytesToRead = std::min(bytesAvailable, byteCount);
	auto cnt2 = mReadPointer + bytesToRead;

	size_t n1, n2;
	if(cnt2 > mCapacityBytes) {
		n1 = mCapacityBytes - mReadPointer;
		n2 = cnt2 & mCapacityBytesMask;
	}
	else {
		n1 = bytesToRead;
		n2 = 0;
	}

	memcpy(destinationBuffer, mBuffer + mReadPointer, n1);
	mReadPointer = (mReadPointer + n1) & mCapacityBytesMask;

	if(n2) {
		memcpy((uint8_t *)destinationBuffer + n1, mBuffer + mReadPointer, n2);
		mReadPointer = (mReadPointer + n2) & mCapacityBytesMask;
	}

	return bytesToRead;
}

size_t SFB::RingBuffer::Peek(void *destinationBuffer, size_t byteCount) const
{
	if(nullptr == destinationBuffer || 0 == byteCount)
		return 0;

	auto bytesAvailable = GetBytesAvailableToRead();
	if(0 == bytesAvailable)
		return 0;

	auto readPointer = mReadPointer;

	auto bytesToRead = std::min(bytesAvailable, byteCount);
	auto cnt2 = readPointer + bytesToRead;

	size_t n1, n2;
	if(cnt2 > mCapacityBytes) {
		n1 = mCapacityBytes - mReadPointer;
		n2 = cnt2 & mCapacityBytesMask;
	}
	else {
		n1 = bytesToRead;
		n2 = 0;
	}

	memcpy(destinationBuffer, mBuffer + readPointer, n1);
	readPointer = (readPointer + n1) & mCapacityBytesMask;

	if(n2)
		memcpy((uint8_t *)destinationBuffer + n1, mBuffer + readPointer, n2);

	return bytesToRead;
}

size_t SFB::RingBuffer::Write(const void *sourceBuffer, size_t byteCount)
{
	if(nullptr == sourceBuffer || 0 == byteCount)
		return 0;

	auto bytesAvailable = GetBytesAvailableToWrite();
	if(0 == bytesAvailable)
		return 0;

	auto bytesToWrite = std::min(bytesAvailable, byteCount);
	auto cnt2 = mWritePointer + bytesToWrite;

	size_t n1, n2;
	if(cnt2 > mCapacityBytes) {
		n1 = mCapacityBytes - mWritePointer;
		n2 = cnt2 & mCapacityBytesMask;
	}
	else {
		n1 = bytesToWrite;
		n2 = 0;
	}

	memcpy(mBuffer + mWritePointer, sourceBuffer, n1);
	mWritePointer = (mWritePointer + n1) & mCapacityBytesMask;

	if(n2) {
		memcpy(mBuffer + mWritePointer, (int8_t *)sourceBuffer + n1, n2);
		mWritePointer = (mWritePointer + n2) & mCapacityBytesMask;
	}

	return bytesToWrite;
}

void SFB::RingBuffer::ReadAdvance(size_t byteCount)
{
	mReadPointer = (mReadPointer + byteCount) & mCapacityBytesMask;
}

void SFB::RingBuffer::WriteAdvance(size_t byteCount)
{
	mWritePointer = (mWritePointer + byteCount) & mCapacityBytesMask;;
}

SFB::RingBuffer::BufferPair SFB::RingBuffer::GetReadVector() const
{
	auto w = mWritePointer;
	auto r = mReadPointer;

	size_t free_cnt;
	if(w > r)
		free_cnt = w - r;
	else
		free_cnt = (w - r + mCapacityBytes) & mCapacityBytesMask;

	auto cnt2 = r + free_cnt;

	if(cnt2 > mCapacityBytes)
		return { { mBuffer + r, mCapacityBytes - r }, { mBuffer, cnt2 & mCapacityBytes } };
	else
		return { { mBuffer + r, free_cnt }, {} };

	return {};
}

SFB::RingBuffer::BufferPair SFB::RingBuffer::GetWriteVector() const
{
	auto w = mWritePointer;
	auto r = mReadPointer;

	size_t free_cnt;
	if(w > r)
		free_cnt = ((r - w + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(w < r)
		free_cnt = (r - w) - 1;
	else
		free_cnt = mCapacityBytes - 1;

	auto cnt2 = w + free_cnt;

	if(cnt2 > mCapacityBytes)
		return { { mBuffer + w, mCapacityBytes - w }, { mBuffer, cnt2 & mCapacityBytes } };
	else
		return { { mBuffer + w, free_cnt }, {} };

	return {};
}
