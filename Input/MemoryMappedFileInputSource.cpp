/*
 *  Copyright (C) 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "MemoryMappedFileInputSource.h"
#include "Logger.h"

#pragma mark Creation and Destruction

MemoryMappedFileInputSource::MemoryMappedFileInputSource(CFURLRef url)
	: InputSource(url), mMemory(nullptr), mCurrentPosition(nullptr)
{
	memset(&mFilestats, 0, sizeof(mFilestats));
}

MemoryMappedFileInputSource::~MemoryMappedFileInputSource()
{
	if(IsOpen())
		Close();
}

bool MemoryMappedFileInputSource::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.MemoryMappedFile", "Open() called on an InputSource that is already open");
		return true;
	}

	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX);
	if(!success) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EIO, nullptr);
		return false;
	}

	int fd = open(reinterpret_cast<const char *>(buf), O_RDONLY);

	if(-1 == fd) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);
		return false;
	}

	if(-1 == fstat(fd, &mFilestats)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);

		if(-1 == close(fd))
			LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.MemoryMappedFile", "Unable to close the file: " << strerror(errno));

		return false;
	}
	
	// Only regular files can be mapped
	if(!S_ISREG(mFilestats.st_mode)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EBADF, nullptr);
		
		if(-1 == close(fd))
			LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.MemoryMappedFile", "Unable to close the file: " << strerror(errno));

		memset(&mFilestats, 0, sizeof(mFilestats));

		return false;
	}

	mMemory = static_cast<int8_t *>(mmap(0, mFilestats.st_size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0));

	if(MAP_FAILED == mMemory) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);

		if(-1 == close(fd))
			LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.MemoryMappedFile", "Unable to close the file: " << strerror(errno));

		memset(&mFilestats, 0, sizeof(mFilestats));

		return false;
	}

	if(-1 == close(fd)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);

		memset(&mFilestats, 0, sizeof(mFilestats));

		return false;
	}

	mCurrentPosition = mMemory;

	mIsOpen = true;
	return true;
}

bool MemoryMappedFileInputSource::Close(CFErrorRef *error)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.MemoryMappedFile", "Close() called on an InputSource that hasn't been opened");
		return true;
	}

	memset(&mFilestats, 0, sizeof(mFilestats));

	if(nullptr != mMemory) {
		int result = munmap(mMemory, mFilestats.st_size);

		mMemory = nullptr;
		mCurrentPosition = nullptr;

		if(-1 == result) {
			if(error)
				*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);
			return false; 
		}
	}

	mIsOpen = false;
	return true;
}

SInt64 MemoryMappedFileInputSource::Read(void *buffer, SInt64 byteCount)
{
	if(!IsOpen() || nullptr == buffer)
		return -1;

	ptrdiff_t remaining = (mMemory + mFilestats.st_size) - mCurrentPosition;

	if(byteCount > remaining)
		byteCount = remaining;

	memcpy(buffer, mCurrentPosition, byteCount);
	mCurrentPosition += byteCount;
	return byteCount;
}

bool MemoryMappedFileInputSource::SeekToOffset(SInt64 offset)
{
	if(!IsOpen())
		return false;

	if(offset > mFilestats.st_size)
		return false;

	mCurrentPosition = mMemory + offset;
	return true;
}
