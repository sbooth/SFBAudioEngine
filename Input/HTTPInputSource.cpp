/*
 *  Copyright (C) 2010, 2011, 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
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
namespace {

	void myCFReadStreamClientCallBack(CFReadStreamRef stream, CFStreamEventType type, void *clientCallBackInfo)
	{
		assert(nullptr != clientCallBackInfo);

		SFB::HTTPInputSource *inputSource = static_cast<SFB::HTTPInputSource *>(clientCallBackInfo);
		inputSource->HandleNetworkEvent(stream, type);
	}

}


#pragma mark Creation and Destruction


SFB::HTTPInputSource::HTTPInputSource(CFURLRef url)
	: InputSource(url), mRequest(nullptr), mReadStream(nullptr), mResponseHeaders(nullptr), mEOSReached(false), mOffset(-1), mDesiredOffset(0)
{}

bool SFB::HTTPInputSource::_Open(CFErrorRef *error)
{
	// Set up the HTTP request
	mRequest = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("GET"), GetURL(), kCFHTTPVersion1_1);
	if(!mRequest) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	CFHTTPMessageSetHeaderFieldValue(mRequest, CFSTR("User-Agent"), CFSTR("SFBAudioEngine"));

	// Seek support
	if(0 < mDesiredOffset) {
		SFB::CFString byteRange = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("bytes=%lld-"), mDesiredOffset);
		CFHTTPMessageSetHeaderFieldValue(mRequest, CFSTR("Range"), byteRange);
	}

	mReadStream = CFReadStreamCreateForStreamedHTTPRequest(kCFAllocatorDefault, mRequest, nullptr);
	if(!mReadStream) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	// Start the HTTP connection
	CFStreamClientContext myContext = {
		.version = 0,
		.info = this,
		.retain = nullptr,
		.release = nullptr,
		.copyDescription = nullptr
	};

	CFOptionFlags clientFlags = kCFStreamEventOpenCompleted | kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered;
    if(!CFReadStreamSetClient(mReadStream, clientFlags, myCFReadStreamClientCallBack, &myContext)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	CFReadStreamScheduleWithRunLoop(mReadStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	if(!CFReadStreamOpen(mReadStream)) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

	while(nullptr == mResponseHeaders)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);

	return true;
}

bool SFB::HTTPInputSource::_Close(CFErrorRef */*error*/)
{
	mRequest = nullptr;
	mReadStream = nullptr;
	mResponseHeaders = nullptr;

	mOffset = -1;
	mDesiredOffset = 0;

	return true;
}

SInt64 SFB::HTTPInputSource::_Read(void *buffer, SInt64 byteCount)
{
	CFStreamStatus status = CFReadStreamGetStatus(mReadStream);
		
	if(kCFStreamStatusAtEnd == status)
		return 0;
	else if(kCFStreamStatusNotOpen == status || kCFStreamStatusClosed == status || kCFStreamStatusError == status)
		return -1;

	CFIndex bytesRead = CFReadStreamRead(mReadStream, (UInt8 *)buffer, (CFIndex)byteCount);

	mOffset += bytesRead;

	return bytesRead;
}

SInt64 SFB::HTTPInputSource::_GetLength() const
{
	if(!mResponseHeaders)
		return -1;

	SInt64 contentLength = -1;

	// FIXME: 64-bit lengths aren't handled correctly
	CFStringRef contentLengthString = reinterpret_cast<CFStringRef>(CFDictionaryGetValue(mResponseHeaders, CFSTR("Content-Length")));
	if(contentLengthString)
		contentLength = CFStringGetIntValue(contentLengthString);

	return contentLength;
}

bool SFB::HTTPInputSource::_SeekToOffset(SInt64 offset)
{
	if(!_Close(nullptr))
		return false;

	mDesiredOffset = offset;
	return _Open(nullptr);
}

CFStringRef SFB::HTTPInputSource::CopyContentMIMEType() const
{
	if(!IsOpen() || !mResponseHeaders)
		return nullptr;

	return reinterpret_cast<CFStringRef>(CFDictionaryGetValue(mResponseHeaders, CFSTR("Content-Type")));
}

void SFB::HTTPInputSource::HandleNetworkEvent(CFReadStreamRef stream, CFStreamEventType type)
{
	switch(type) {
		case kCFStreamEventOpenCompleted:
			mOffset = mDesiredOffset;
			break;

		case kCFStreamEventHasBytesAvailable:
			if(nullptr == mResponseHeaders) {
				SFB::CFType responseHeader = CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader);
				if(responseHeader) {
					mResponseHeaders = CFHTTPMessageCopyAllHeaderFields((CFHTTPMessageRef)responseHeader.Object());
				}
			}
			break;
		
		case kCFStreamEventErrorOccurred:
		{
			SFB::CFError error = CFReadStreamCopyError(stream);
			if(error)
				LOGGER_ERR("org.sbooth.AudioEngine.InputSource.HTTP", "Error: " << error);
			break;
		}

		case kCFStreamEventEndEncountered:
			mEOSReached = true;
			break;
	}
}
