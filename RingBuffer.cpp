/*
 *  Copyright (C) 2014 Stephen F. Booth <me@sbooth.org>
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
	: mBuffer(nullptr), mCapacityBytes(0), mReadPointer(0), mWritePointer(0)
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

	mBuffer = (int8_t *)malloc(mCapacityBytes);
	if(nullptr == mBuffer)
		return false;

	memset(mBuffer, 0, mCapacityBytes);

	mReadPointer = 0;
	mWritePointer = 0;

	return true;
}

void SFB::RingBuffer::Deallocate()
{
	if(mBuffer)
		free(mBuffer), mBuffer = nullptr;
}


void SFB::RingBuffer::Reset()
{
	mReadPointer = 0;
	mWritePointer = 0;

	memset(mBuffer, 0, mCapacityBytes);
}

size_t SFB::RingBuffer::GetBytesAvailableToRead() const
{
	size_t w = mWritePointer;
	size_t r = mReadPointer;

	if(w > r)
		return w - r;
	else
		return (w - r + mCapacityBytes) & mCapacityBytesMask;
}

size_t SFB::RingBuffer::GetBytesAvailableToWrite() const
{
	size_t w = mWritePointer;
	size_t r = mReadPointer;

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

	size_t bytesAvailable = GetBytesAvailableToRead();
	if(0 == bytesAvailable)
		return 0;

	size_t bytesToRead = std::min(bytesAvailable, byteCount);
	size_t cnt2 = mReadPointer + bytesToRead;

	size_t n1, n2;
	if(cnt2 > mCapacityBytes) {
		n1 = mCapacityBytes - mReadPointer;
		n2 = cnt2 & mCapacityBytesMask;
	}
	else {
		n1 = bytesToRead;
		n2 = 0;
	}

	memcpy(destinationBuffer, mBuffer, n1);
	mReadPointer = (mReadPointer + n1) & mCapacityBytesMask;

	if(n2) {
		memcpy((int8_t *)destinationBuffer + n1, mBuffer + mReadPointer, n2);
		mReadPointer = (mReadPointer + n2) & mCapacityBytesMask;
	}

	return bytesToRead;
}

size_t SFB::RingBuffer::Write(const void *sourceBuffer, size_t byteCount)
{
	if(nullptr == sourceBuffer || 0 == byteCount)
		return 0;

	size_t bytesAvailable = GetBytesAvailableToWrite();
	if(0 == bytesAvailable)
		return 0;

	size_t bytesToWrite = std::min(bytesAvailable, byteCount);
	size_t cnt2 = mWritePointer + bytesToWrite;

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
