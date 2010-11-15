/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include <log4cxx/logger.h>

#include "InMemoryFileInputSource.h"

#pragma mark Creation and Destruction

InMemoryFileInputSource::InMemoryFileInputSource(CFURLRef url)
	: InputSource(url), mMemory(NULL), mCurrentPosition(NULL)
{
	memset(&mFilestats, 0, sizeof(mFilestats));
}

InMemoryFileInputSource::~InMemoryFileInputSource()
{
	if(IsOpen())
		Close();
}

bool InMemoryFileInputSource::Open(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX);
	if(!success) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EIO, NULL);
		return false;
	}
	
	int fd = open(reinterpret_cast<const char *>(buf), O_RDONLY);
	
	if(-1 == fd) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);
		return false;
	}
	
	if(-1 == fstat(fd, &mFilestats)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);
		
		if(-1 == close(fd)) {
			log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.InMemoryFileInputSource");
			LOG4CXX_WARN(logger, "Unable to close the file: " << strerror(errno));
		}
		
		return false;
	}
	
	// Perform the allocation
	mMemory = static_cast<int8_t *>(malloc(mFilestats.st_size));

	if(NULL == mMemory) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);
		
		if(-1 == close(fd)) {
			log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.InMemoryFileInputSource");
			LOG4CXX_WARN(logger, "Unable to close the file: " << strerror(errno));
		}
		
		memset(&mFilestats, 0, sizeof(mFilestats));

		return false;
	}

	// Read the file
	if(-1 == read(fd, mMemory, mFilestats.st_size)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);
		
		if(-1 == close(fd)) {
			log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.InMemoryFileInputSource");
			LOG4CXX_WARN(logger, "Unable to close the file: " << strerror(errno));
		}
		
		memset(&mFilestats, 0, sizeof(mFilestats));

		return false;
	}

	if(-1 == close(fd)) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.InMemoryFileInputSource");
		LOG4CXX_ERROR(logger, "Unable to close the file: " << strerror(errno));

		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);
		
		memset(&mFilestats, 0, sizeof(mFilestats));
		
		return false;
	}
	
	mCurrentPosition = mMemory;
	
	return true;
}

bool InMemoryFileInputSource::Close(CFErrorRef *error)
{
#pragma unused(error)

	memset(&mFilestats, 0, sizeof(mFilestats));
	
	if(NULL != mMemory)
		free(mMemory), mMemory = NULL;

	mCurrentPosition = NULL;
	
	return true;
}

SInt64 InMemoryFileInputSource::Read(void *buffer, SInt64 byteCount)
{
	assert(NULL != buffer);
	
	ptrdiff_t remaining = (mMemory + mFilestats.st_size) - mCurrentPosition;
	
	if(byteCount > remaining)
		byteCount = remaining;
	
	memcpy(buffer, mCurrentPosition, byteCount);
	mCurrentPosition += byteCount;
	return byteCount;
}

bool InMemoryFileInputSource::SeekToOffset(SInt64 offset)
{
	if(offset > mFilestats.st_size)
		return false;
	
	mCurrentPosition = mMemory + offset;
	return true;
}
