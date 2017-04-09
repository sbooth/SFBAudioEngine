/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <fcntl.h>
#include <unistd.h>

#include "FileInputSource.h"

#pragma mark Creation and Destruction

SFB::FileInputSource::FileInputSource(CFURLRef url)
	: InputSource(url), mFile(nullptr, nullptr)
{
	memset(&mFilestats, 0, sizeof(mFilestats));
}

bool SFB::FileInputSource::_Open(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	Boolean success = CFURLGetFileSystemRepresentation(GetURL(), FALSE, buf, PATH_MAX);
	if(!success) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EIO, nullptr);
		return false;
	}

	mFile = unique_FILE_ptr(std::fopen((const char *)buf, "r"), std::fclose);
	if(!mFile) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);
		return false;
	}

	if(-1 == stat((const char *)buf, &mFilestats)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, errno, nullptr);

		mFile.reset();

		return false;
	}

	return true;
}

bool SFB::FileInputSource::_Close(CFErrorRef *error)
{
#pragma unused(error)

	memset(&mFilestats, 0, sizeof(mFilestats));
	mFile.reset();

	return true;
}
