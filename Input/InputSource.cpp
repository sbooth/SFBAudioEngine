/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "FileInputSource.h"
#include "HTTPInputSource.h"
#include "InMemoryFileInputSource.h"
#include "InputSource.h"
#include "MemoryInputSource.h"
#include "MemoryMappedFileInputSource.h"

// ========================================
// Error Codes
// ========================================
const CFStringRef SFB::InputSource::ErrorDomain = CFSTR("org.sbooth.AudioEngine.ErrorDomain.InputSource");

#pragma mark Static Methods

SFB::InputSource::unique_ptr SFB::InputSource::CreateForURL(CFURLRef url, int flags, CFErrorRef *error)
{
	if(nullptr == url)
		return nullptr;

	// If there is no scheme the URL is invalid
	SFB::CFString scheme(CFURLCopyScheme(url));
	if(!scheme) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EINVAL, nullptr);
		return nullptr;
	}

	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		if(InputSource::MemoryMapFiles & flags)
			return unique_ptr(new MemoryMappedFileInputSource(url));
		else if(InputSource::LoadFilesInMemory & flags)
			return unique_ptr(new InMemoryFileInputSource(url));
		else
			return unique_ptr(new FileInputSource(url));
	}
	else if(kCFCompareEqualTo == CFStringCompare(CFSTR("http"), scheme, kCFCompareCaseInsensitive)
            || kCFCompareEqualTo == CFStringCompare(CFSTR("https"), scheme, kCFCompareCaseInsensitive))
		return unique_ptr(new HTTPInputSource(url));

	return nullptr;
}

SFB::InputSource::unique_ptr SFB::InputSource::CreateWithMemory(const void *bytes, SInt64 byteCount, bool copyBytes, CFErrorRef *error)
{
#pragma unused(error)

	if(nullptr == bytes || 0 >= byteCount)
		return nullptr;

	return unique_ptr(new MemoryInputSource(bytes, byteCount, copyBytes));
}

#pragma mark Creation and Destruction

SFB::InputSource::InputSource()
	: mURL(nullptr), mIsOpen(false)
{}

SFB::InputSource::InputSource(CFURLRef url)
	: mURL((CFURLRef)CFRetain(url)), mIsOpen(false)
{
	assert(nullptr != url);
}

bool SFB::InputSource::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		os_log_debug(OS_LOG_DEFAULT, "Open() called on an InputSource that is already open");
		return true;
	}

	bool result = _Open(error);
	if(result)
		mIsOpen = true;
	return result;
}

bool SFB::InputSource::Close(CFErrorRef *error)
{
	if(!IsOpen()) {
		os_log_debug(OS_LOG_DEFAULT, "Close() called on an InputSource that hasn't been opened");
		return true;
	}

	bool result = _Close(error);
	if(result)
		mIsOpen = false;
	return result;
}

SInt64 SFB::InputSource::Read(void *buffer, SInt64 byteCount)
{
	if(!IsOpen() || nullptr == buffer || 0 > byteCount) {
		os_log_debug(OS_LOG_DEFAULT, "Read() called on an InputSource that hasn't been opened");
		return -1;
	}

	return _Read(buffer, byteCount);
}

bool SFB::InputSource::AtEOF() const
{
	if(!IsOpen()) {
		os_log_debug(OS_LOG_DEFAULT, "AtEOF() called on an InputSource that hasn't been opened");
		return true;
	}

	return _AtEOF();
}

SInt64 SFB::InputSource::GetOffset() const
{
	if(!IsOpen()) {
		os_log_debug(OS_LOG_DEFAULT, "GetOffset() called on an InputSource that hasn't been opened");
		return -1;
	}

	return _GetOffset();
}

SInt64 SFB::InputSource::GetLength() const
{
	if(!IsOpen()) {
		os_log_debug(OS_LOG_DEFAULT, "GetLength() called on an InputSource that hasn't been opened");
		return 0;
	}

	return _GetLength();
}

bool SFB::InputSource::SupportsSeeking() const
{
	if(!IsOpen()) {
		os_log_debug(OS_LOG_DEFAULT, "SupportsSeeking() called on an InputSource that hasn't been opened");
		return false;
	}

	return _SupportsSeeking();
}

bool SFB::InputSource::SeekToOffset(SInt64 offset)
{
	if(!IsOpen() || 0 > offset) {
		os_log_debug(OS_LOG_DEFAULT, "Close() called on an InputSource that hasn't been opened");
		return false;
	}

	return _SeekToOffset(offset);
}
