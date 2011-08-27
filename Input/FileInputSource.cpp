/*
 *  Copyright (C) 2010, 2011 Stephen F. Booth <me@sbooth.org>
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

#include "FileInputSource.h"
#include "Logger.h"

#pragma mark Creation and Destruction

FileInputSource::FileInputSource(CFURLRef url)
	: InputSource(url), mFile(NULL)
{
	memset(&mFilestats, 0, sizeof(mFilestats));
}

FileInputSource::~FileInputSource()
{
	if(IsOpen())
		Close();
}

bool FileInputSource::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.File", "Open() called on an InputSource that is already open");
		return true;
	}

	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(mURL, FALSE, buf, PATH_MAX);
	if(!success) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EIO, NULL);
		return false;
	}

	mFile = fopen(reinterpret_cast<const char *>(buf), "r");

	if(NULL == mFile) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);
		return false;
	}

	if(-1 == stat(reinterpret_cast<const char *>(buf), &mFilestats)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);

		if(0 != fclose(mFile))
			LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.File", "Unable to close the file: " << strerror(errno));

		mFile = NULL;

		return false;
	}

	mIsOpen = true;
	return true;
}

bool FileInputSource::Close(CFErrorRef *error)
{
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.File", "Close() called on an InputSource that hasn't been opened");
		return true;
	}

	memset(&mFilestats, 0, sizeof(mFilestats));

	if(NULL != mFile) {
		int result = fclose(mFile);

		mFile = NULL;

		if(-1 == result) {
			if(error)
				*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, NULL);
			return false;
		}
	}

	mIsOpen = false;
	return true;
}

SInt64 FileInputSource::Read(void *buffer, SInt64 byteCount)
{
	if(!IsOpen() || NULL == buffer)
		return -1;

	return fread(buffer, 1, byteCount, mFile);
}

bool FileInputSource::SeekToOffset(SInt64 offset)
{
	if(!IsOpen())
		return false;

	return (0 == fseeko(mFile, offset, SEEK_SET));
}
