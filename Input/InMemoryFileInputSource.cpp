/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <cstdio>
#include <functional>

#include "InMemoryFileInputSource.h"

#pragma mark Creation and Destruction

SFB::InMemoryFileInputSource::InMemoryFileInputSource(CFURLRef url)
	: InputSource(url), mMemory(nullptr), mCurrentPosition(nullptr)
{
	memset(&mFilestats, 0, sizeof(mFilestats));
}

bool SFB::InMemoryFileInputSource::_Open(CFErrorRef *error)
{
	using unique_FILE_ptr = std::unique_ptr<std::FILE, std::function<int(std::FILE *)>>;

	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(GetURL(), FALSE, buf, PATH_MAX);
	if(!success) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EIO, nullptr);
		return false;
	}

	auto file = unique_FILE_ptr(std::fopen((const char *)buf, "r"), std::fclose);
	if(!file) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);
		return false;
	}

	if(-1 == fstat(::fileno(file.get()), &mFilestats)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);
		return false;
	}

	// Perform the allocation
	mMemory = std::unique_ptr<int8_t []>(new int8_t [(size_t)mFilestats.st_size]);
	if(!mMemory) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);
		return false;
	}

	// Read the file
	if((size_t)mFilestats.st_size != ::fread(mMemory.get(), 1, (size_t)mFilestats.st_size, file.get())) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);

		return false;
	}

	mCurrentPosition = mMemory.get();

	return true;
}

bool SFB::InMemoryFileInputSource::_Close(CFErrorRef */*error*/)
{
	memset(&mFilestats, 0, sizeof(mFilestats));
	mMemory.reset();
	mCurrentPosition = nullptr;

	return true;
}

SInt64 SFB::InMemoryFileInputSource::_Read(void *buffer, SInt64 byteCount)
{
	ptrdiff_t remaining = (mMemory.get() + mFilestats.st_size) - mCurrentPosition;

	if(byteCount > remaining)
		byteCount = remaining;

	memcpy(buffer, mCurrentPosition, (size_t)byteCount);
	mCurrentPosition += byteCount;
	return byteCount;
}

bool SFB::InMemoryFileInputSource::_SeekToOffset(SInt64 offset)
{
	if(offset > mFilestats.st_size)
		return false;

	mCurrentPosition = mMemory.get() + offset;
	return true;
}
