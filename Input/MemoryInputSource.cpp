/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <stdexcept>

#include "MemoryInputSource.h"

#pragma mark Creation and Destruction

SFB::MemoryInputSource::MemoryInputSource(const void *bytes, SInt64 byteCount, bool copyBytes)
	: InputSource(), mByteCount(byteCount), mMemory(nullptr, [](int8_t * /*buf*/) {}), mCurrentPosition(nullptr)
{
	if(0 >= byteCount)
		throw std::runtime_error("byteCount must be positive");

	if(copyBytes) {
		void *allocation = malloc((size_t)byteCount);
		if(nullptr == allocation)
			throw std::bad_alloc();
		mMemory = unique_mem_ptr((int8_t *)allocation, [](int8_t *buf) {
			free(buf);
		});
		memcpy(mMemory.get(), bytes, (size_t)byteCount);
	}
	else
		mMemory = unique_mem_ptr((int8_t *)bytes, [](int8_t * /*buf*/) {});

}

bool SFB::MemoryInputSource::_Open(CFErrorRef *error)
{
#pragma unused(error)

	mCurrentPosition = mMemory.get();
	return true;
}

bool SFB::MemoryInputSource::_Close(CFErrorRef *error)
{
#pragma unused(error)

	mCurrentPosition = nullptr;
	return true;
}

SInt64 SFB::MemoryInputSource::_Read(void *buffer, SInt64 byteCount)
{
	ptrdiff_t remaining = (mMemory.get() + mByteCount) - mCurrentPosition;

	if(byteCount > remaining)
		byteCount = remaining;

	memcpy(buffer, mCurrentPosition, (size_t)byteCount);
	mCurrentPosition += byteCount;
	return byteCount;
}

bool SFB::MemoryInputSource::_SeekToOffset(SInt64 offset)
{
	if(offset > mByteCount)
		return false;

	mCurrentPosition = mMemory.get() + offset;
	return true;
}
