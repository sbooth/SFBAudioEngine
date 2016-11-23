/*
 *  Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
