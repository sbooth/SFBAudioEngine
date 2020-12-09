/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <algorithm>
#include <cstdlib>

#include "RingBuffer.h"

namespace {

	/*!
	 * Return the smallest power of two value greater than \c x
	 * @param x A value in the range [2..2147483648]
	 * @return The smallest power of two greater than \c x
	 */
	inline constexpr uint32_t NextPowerOfTwo(uint32_t x) noexcept
	{
		assert(x > 1);
		assert(x <= ((UINT32_MAX / 2) + 1));
		return (uint32_t)1 << (32 - __builtin_clz(x - 1));
	}

}

#pragma mark Creation and Destruction

SFB::RingBuffer::RingBuffer() noexcept
	: mBuffer(nullptr), mCapacityBytes(0), mCapacityBytesMask(0), mWritePosition(0), mReadPosition(0)
{
	assert(mWritePosition.is_lock_free());
}

SFB::RingBuffer::~RingBuffer() noexcept
{
	Deallocate();
}

#pragma mark Buffer Management

bool SFB::RingBuffer::Allocate(size_t capacityBytes) noexcept
{
	if(capacityBytes < 2 || capacityBytes > 0x80000000)
		return false;

	Deallocate();

	// Round up to the next power of two
	capacityBytes = NextPowerOfTwo((uint32_t)capacityBytes);

	mBuffer = (uint8_t *)malloc(capacityBytes);
	if(!mBuffer)
		return false;

	mCapacityBytes = capacityBytes;
	mCapacityBytesMask = capacityBytes - 1;

	return true;
}

void SFB::RingBuffer::Deallocate() noexcept
{
	if(mBuffer) {
		free(mBuffer);
		mBuffer = nullptr;

		mCapacityBytes = 0;
		mCapacityBytesMask = 0;

		mReadPosition = 0;
		mWritePosition = 0;
	}
}

void SFB::RingBuffer::Reset() noexcept
{
	mReadPosition = 0;
	mWritePosition = 0;
}

size_t SFB::RingBuffer::BytesAvailableToRead() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	if(writePosition > readPosition)
		return writePosition - readPosition;
	else
		return (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;
}

size_t SFB::RingBuffer::BytesAvailableToWrite() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	if(writePosition > readPosition)
		return ((readPosition - writePosition + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(writePosition < readPosition)
		return (readPosition - writePosition) - 1;
	else
		return mCapacityBytes - 1;
}

size_t SFB::RingBuffer::Read(void *destinationBuffer, size_t byteCount) noexcept
{
	if(nullptr == destinationBuffer || 0 == byteCount)
		return 0;

	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	size_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = writePosition - readPosition;
	else
		bytesAvailable = (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;

	if(0 == bytesAvailable)
		return 0;

	size_t bytesToRead = std::min(bytesAvailable, byteCount);
	if(readPosition + bytesToRead > mCapacityBytes) {
		auto bytesAfterReadPointer = mCapacityBytes - readPosition;
		memcpy(destinationBuffer, mBuffer + readPosition, bytesAfterReadPointer);
		memcpy((uint8_t *)destinationBuffer + bytesAfterReadPointer, mBuffer, bytesToRead - bytesAfterReadPointer);
	}
	else
		memcpy(destinationBuffer, mBuffer + readPosition, bytesToRead);

	mReadPosition.store((readPosition + bytesToRead) & mCapacityBytesMask, std::memory_order_release);

	return bytesToRead;
}

size_t SFB::RingBuffer::Peek(void *destinationBuffer, size_t byteCount) const noexcept
{
	if(nullptr == destinationBuffer || 0 == byteCount)
		return 0;

	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	size_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = writePosition - readPosition;
	else
		bytesAvailable = (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;

	if(0 == bytesAvailable)
		return 0;

	size_t bytesToRead = std::min(bytesAvailable, byteCount);
	if(readPosition + bytesToRead > mCapacityBytes) {
		auto bytesAfterReadPointer = mCapacityBytes - readPosition;
		memcpy(destinationBuffer, mBuffer + readPosition, bytesAfterReadPointer);
		memcpy((uint8_t *)destinationBuffer + bytesAfterReadPointer, mBuffer, bytesToRead - bytesAfterReadPointer);
	}
	else
		memcpy(destinationBuffer, mBuffer + readPosition, bytesToRead);

	return bytesToRead;
}

size_t SFB::RingBuffer::Write(const void *sourceBuffer, size_t byteCount) noexcept
{
	if(nullptr == sourceBuffer || 0 == byteCount)
		return 0;

	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	size_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = ((readPosition - writePosition + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(writePosition < readPosition)
		bytesAvailable = (readPosition - writePosition) - 1;
	else
		bytesAvailable = mCapacityBytes - 1;

	if(0 == bytesAvailable)
		return 0;

	size_t bytesToWrite = std::min(bytesAvailable, byteCount);
	if(writePosition + bytesToWrite > mCapacityBytes) {
		auto bytesAfterWritePointer = mCapacityBytes - writePosition;
		memcpy(mBuffer + writePosition, sourceBuffer, bytesAfterWritePointer);
		memcpy(mBuffer, (uint8_t *)sourceBuffer + bytesAfterWritePointer, bytesToWrite - bytesAfterWritePointer);
	}
	else
		memcpy(mBuffer + writePosition, sourceBuffer, bytesToWrite);

	mWritePosition.store((writePosition + bytesToWrite) & mCapacityBytesMask, std::memory_order_release);

	return bytesToWrite;
}

void SFB::RingBuffer::AdvanceReadPosition(size_t byteCount) noexcept
{
	mReadPosition.store((mReadPosition.load(std::memory_order_acquire) + byteCount) & mCapacityBytesMask, std::memory_order_release);
}

void SFB::RingBuffer::AdvanceWritePosition(size_t byteCount) noexcept
{
	mWritePosition.store((mWritePosition.load(std::memory_order_acquire) + byteCount) & mCapacityBytesMask, std::memory_order_release);
}

SFB::RingBuffer::BufferPair SFB::RingBuffer::ReadVector() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	size_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = writePosition - readPosition;
	else
		bytesAvailable = (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;

	auto endOfRead = readPosition + bytesAvailable;

	SFB::RingBuffer::BufferPair readVector;
	if(endOfRead > mCapacityBytes)
		readVector = { { mBuffer + readPosition, mCapacityBytes - readPosition }, { mBuffer, endOfRead & mCapacityBytes } };
	else
		readVector = { { mBuffer + readPosition, bytesAvailable }, {} };

	return readVector;
}

SFB::RingBuffer::BufferPair SFB::RingBuffer::WriteVector() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	size_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = ((readPosition - writePosition + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(writePosition < readPosition)
		bytesAvailable = (readPosition - writePosition) - 1;
	else
		bytesAvailable = mCapacityBytes - 1;

	auto endOfWrite = writePosition + bytesAvailable;

	SFB::RingBuffer::BufferPair writeVector;
	if(endOfWrite > mCapacityBytes)
		writeVector = { { mBuffer + writePosition, mCapacityBytes - writePosition }, { mBuffer, endOfWrite & mCapacityBytes } };
	else
		writeVector = { { mBuffer + writePosition, bytesAvailable }, {} };

	return writeVector;
}
