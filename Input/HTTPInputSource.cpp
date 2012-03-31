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

#include "HTTPInputSource.h"
#include "Logger.h"

// ========================================
// CFNetwork callbacks
// ========================================
static void myCFReadStreamClientCallBack(CFReadStreamRef stream, CFStreamEventType type, void *clientCallBackInfo)
{
	assert(nullptr != clientCallBackInfo);
	
	HTTPInputSource *inputSource = static_cast<HTTPInputSource *>(clientCallBackInfo);
	inputSource->HandleNetworkEvent(stream, type);
}


#pragma mark Creation and Destruction


HTTPInputSource::HTTPInputSource(CFURLRef url)
	: InputSource(url), mRequest(nullptr), mReadStream(nullptr), mResponseHeaders(nullptr), mEOSReached(false), mOffset(-1), mDesiredOffset(0)
{}

HTTPInputSource::~HTTPInputSource()
{
	if(IsOpen())
		Close();
}

bool HTTPInputSource::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.HTTP", "Open() called on an InputSource that is already open");
		return true;
	}

	// Set up the HTTP request
	mRequest = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("GET"), mURL, kCFHTTPVersion1_1);
	if(nullptr == mRequest) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	CFHTTPMessageSetHeaderFieldValue(mRequest, CFSTR("User-Agent"), CFSTR("SFBAudioEngine"));

	// Seek support
	if(0 < mDesiredOffset) {
		CFStringRef byteRange = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("bytes=%ld-"), mDesiredOffset);
		CFHTTPMessageSetHeaderFieldValue(mRequest, CFSTR("Range"), byteRange);
		CFRelease(byteRange), byteRange = nullptr;
	}

	mReadStream = CFReadStreamCreateForStreamedHTTPRequest(kCFAllocatorDefault, mRequest, nullptr);
	if(nullptr == mReadStream) {
		CFRelease(mRequest), mRequest = nullptr;
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	// Start the HTTP connection
	CFStreamClientContext myContext = { 0, this, nullptr, nullptr, nullptr };

	CFOptionFlags clientFlags = kCFStreamEventOpenCompleted | kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered;
    if(!CFReadStreamSetClient(mReadStream, clientFlags, myCFReadStreamClientCallBack, &myContext)) {
		CFRelease(mRequest), mRequest = nullptr;
		CFRelease(mReadStream), mReadStream = nullptr;
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	CFReadStreamScheduleWithRunLoop(mReadStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	if(!CFReadStreamOpen(mReadStream)) {
		CFRelease(mRequest), mRequest = nullptr;
		CFRelease(mReadStream), mReadStream = nullptr;
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	while(nullptr == mResponseHeaders)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);

	mIsOpen = true;
	return true;
}

bool HTTPInputSource::Close(CFErrorRef *error)
{
#pragma unused(error)
	if(!IsOpen()) {
		LOGGER_WARNING("org.sbooth.AudioEngine.InputSource.HTTP", "Close() called on an InputSource that hasn't been opened");
		return true;
	}

	if(mRequest)
		CFRelease(mRequest), mRequest = nullptr;
	if(mReadStream)
		CFRelease(mReadStream), mReadStream = nullptr;
	if(mResponseHeaders)
		CFRelease(mResponseHeaders), mResponseHeaders = nullptr;

	mOffset = -1;
	mDesiredOffset = 0;
	mIsOpen = false;

	return true;
}

SInt64 HTTPInputSource::Read(void *buffer, SInt64 byteCount)
{
	if(!IsOpen())
		return -1;

	CFStreamStatus status = CFReadStreamGetStatus(mReadStream);
		
	if(kCFStreamStatusAtEnd == status)
		return 0;
	else if(kCFStreamStatusNotOpen == status || kCFStreamStatusClosed == status || kCFStreamStatusError == status)
		return -1;

	CFIndex bytesRead = CFReadStreamRead(mReadStream, static_cast<UInt8 *>(buffer), byteCount);

	mOffset += bytesRead;

	return bytesRead;
}

SInt64 HTTPInputSource::GetLength() const
{
	if(!IsOpen() || !mResponseHeaders)
		return -1;

	SInt64 contentLength = -1;

	// FIXME: 64-bit lengths aren't handled correctly
	CFStringRef contentLengthString = reinterpret_cast<CFStringRef>(CFDictionaryGetValue(mResponseHeaders, CFSTR("Content-Length")));
	if(contentLengthString)
		contentLength = CFStringGetIntValue(contentLengthString);

	return contentLength;
}

bool HTTPInputSource::SeekToOffset(SInt64 offset)
{
	if(!IsOpen())
		return false;

	if(!Close())
		return false;

	mDesiredOffset = offset;
	return Open();
}

CFStringRef HTTPInputSource::CopyContentMIMEType() const
{
	if(!IsOpen() || !mResponseHeaders)
		return nullptr;

	return reinterpret_cast<CFStringRef>(CFDictionaryGetValue(mResponseHeaders, CFSTR("Content-Type")));
}

void HTTPInputSource::HandleNetworkEvent(CFReadStreamRef stream, CFStreamEventType type)
{
	switch(type) {
		case kCFStreamEventOpenCompleted:
			mOffset = mDesiredOffset;
			break;

		case kCFStreamEventHasBytesAvailable:
			if(nullptr == mResponseHeaders) {
				CFTypeRef responseHeader = CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader);
				if(responseHeader) {
					mResponseHeaders = CFHTTPMessageCopyAllHeaderFields(static_cast<CFHTTPMessageRef>(const_cast<void *>(responseHeader)));
					CFRelease(responseHeader), responseHeader = nullptr;
				}				
			}
			break;
		
		case kCFStreamEventErrorOccurred:
		{
			CFErrorRef error = CFReadStreamCopyError(stream);
			if(error) {
				LOGGER_ERR("org.sbooth.AudioEngine.InputSource.HTTP", "Error: " << error);
				CFRelease(error), error = nullptr;
			}
			break;
		}

		case kCFStreamEventEndEncountered:
			mEOSReached = true;
			break;
	}
}
