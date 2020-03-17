/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <cstdio>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "MemoryMappedFileInputSource.h"

#pragma mark Creation and Destruction

SFB::MemoryMappedFileInputSource::MemoryMappedFileInputSource(CFURLRef url)
	: InputSource(url), mMemory(nullptr), mCurrentPosition(nullptr)
{
	memset(&mFilestats, 0, sizeof(mFilestats));
}

bool SFB::MemoryMappedFileInputSource::_Open(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(GetURL(), FALSE, buf, PATH_MAX);
	if(!success) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EIO, nullptr);
		return false;
	}

	auto file = std::unique_ptr<std::FILE, int (*)(std::FILE *)>(std::fopen((const char *)buf, "r"), std::fclose);
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

	// Only regular files can be mapped
	if(!S_ISREG(mFilestats.st_mode)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EBADF, nullptr);
		return false;
	}

	// Map the file to memory
	size_t map_size = (size_t)mFilestats.st_size;
	mMemory = unique_mappedmem_ptr((int8_t *)mmap(0, map_size, PROT_READ, MAP_FILE | MAP_SHARED, ::fileno(file.get()), 0), std::bind(munmap, std::placeholders::_1, map_size));

	if(MAP_FAILED == mMemory.get()) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);
		return false;
	}

	mCurrentPosition = mMemory.get();

	return true;
}

bool SFB::MemoryMappedFileInputSource::_Close(CFErrorRef *error)
{
#pragma unused(error)

	memset(&mFilestats, 0, sizeof(mFilestats));
	mMemory.reset();
	mCurrentPosition = nullptr;

	return true;
}

SInt64 SFB::MemoryMappedFileInputSource::_Read(void *buffer, SInt64 byteCount)
{
	ptrdiff_t remaining = (mMemory.get() + mFilestats.st_size) - mCurrentPosition;

	if(byteCount > remaining)
		byteCount = remaining;

	memcpy(buffer, mCurrentPosition, (size_t)byteCount);
	mCurrentPosition += byteCount;
	return byteCount;
}

bool SFB::MemoryMappedFileInputSource::_SeekToOffset(SInt64 offset)
{
	if(offset > mFilestats.st_size)
		return false;

	mCurrentPosition = mMemory.get() + offset;
	return true;
}
