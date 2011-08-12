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

#include "InputSource.h"
#include "FileInputSource.h"
#include "MemoryMappedFileInputSource.h"
#include "InMemoryFileInputSource.h"
#include "HTTPInputSource.h"

// ========================================
// Error Codes
// ========================================
const CFStringRef	InputSourceErrorDomain			= CFSTR("org.sbooth.AudioEngine.ErrorDomain.InputSource");

#pragma mark Static Methods

InputSource * InputSource::CreateInputSourceForURL(CFURLRef url, int flags, CFErrorRef *error)
{
	if(NULL == url)
		return NULL;
	
	InputSource *inputSource = NULL;
	
	CFStringRef scheme = CFURLCopyScheme(url);

	// If there is no scheme the URL is invalid
	if(NULL == scheme) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, EINVAL, NULL);
		return NULL;
	}

	if(kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive)) {
		if(InputSourceFlagMemoryMapFiles & flags)
			inputSource = new MemoryMappedFileInputSource(url);
		else if(InputSourceFlagLoadFilesInMemory & flags)
			inputSource = new InMemoryFileInputSource(url);
		else
			inputSource = new FileInputSource(url);
	}
	else if(kCFCompareEqualTo == CFStringCompare(CFSTR("http"), scheme, kCFCompareCaseInsensitive))
		inputSource = new HTTPInputSource(url);

	CFRelease(scheme), scheme = NULL;

	return inputSource;
}

#pragma mark Creation and Destruction

InputSource::InputSource()
	: mURL(NULL), mIsOpen(false)
{}

InputSource::InputSource(CFURLRef url)
	: mURL(NULL), mIsOpen(false)
{
	assert(NULL != url);
	
	mURL = static_cast<CFURLRef>(CFRetain(url));
}

InputSource::InputSource(const InputSource& rhs)
	: mURL(NULL), mIsOpen(false)
{
	*this = rhs;
}

InputSource::~InputSource()
{
	if(mURL)
		CFRelease(mURL), mURL = NULL;
}

#pragma mark Operator Overloads

InputSource& InputSource::operator=(const InputSource& rhs)
{
	if(this == &rhs)
		return *this;

	if(mURL)
		CFRelease(mURL), mURL = NULL;
	
	if(rhs.mURL)
		mURL = static_cast<CFURLRef>(CFRetain(rhs.mURL));

	mIsOpen = rhs.mIsOpen;

	return *this;
}
