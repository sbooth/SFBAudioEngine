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

#include "AudioEngineDefines.h"
#include "HTTPInputSource.h"


// ========================================
// CFNetwork callbacks
// ========================================
static void myCFReadStreamClientCallBack(CFReadStreamRef stream, CFStreamEventType type, void *clientCallBackInfo)
{
	assert(NULL != clientCallBackInfo);
	
	HTTPInputSource *inputSource = static_cast<HTTPInputSource *>(clientCallBackInfo);
	inputSource->HandleNetworkEvent(stream, type);
}


#pragma mark Creation and Destruction


HTTPInputSource::HTTPInputSource(CFURLRef url)
	: InputSource(url), mRequest(NULL), mReadStream(NULL), mResponseHeaders(NULL), mEOSReached(false), mOffset(-1)
{}

HTTPInputSource::~HTTPInputSource()
{
	if(IsOpen())
		Close();
}

bool HTTPInputSource::Open(CFErrorRef *error)
{
	// Set up the HTTP request
	mRequest = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("GET"), mURL, kCFHTTPVersion1_1);
	
	if(NULL == mRequest) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
		return false;
	}

	CFHTTPMessageSetHeaderFieldValue(mRequest, CFSTR("User-Agent"), CFSTR("SFBAudioEngine"));
	
	mReadStream = CFReadStreamCreateForStreamedHTTPRequest(kCFAllocatorDefault, mRequest, NULL);
	
	if(NULL == mReadStream) {
		CFRelease(mRequest), mRequest = NULL;
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
		return false;
	}

	// Start the HTTP connection
	CFStreamClientContext myContext = { 0, this, NULL, NULL, NULL };

	CFOptionFlags clientFlags = kCFStreamEventOpenCompleted | kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered;
    if(!CFReadStreamSetClient(mReadStream, clientFlags, myCFReadStreamClientCallBack, &myContext)) {
		CFRelease(mRequest), mRequest = NULL;
		CFRelease(mReadStream), mReadStream = NULL;
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
		return false;
	}

	CFReadStreamScheduleWithRunLoop(mReadStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	
	if(!CFReadStreamOpen(mReadStream)) {
		CFRelease(mRequest), mRequest = NULL;
		CFRelease(mReadStream), mReadStream = NULL;
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, NULL);
		return false;
	}

	while(NULL == mResponseHeaders)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
	
	return true;
}

bool HTTPInputSource::Close(CFErrorRef *error)
{
#pragma unused(error)
	if(mRequest)
		CFRelease(mRequest), mRequest = NULL;
	if(mReadStream)
		CFRelease(mReadStream), mReadStream = NULL;
	if(mResponseHeaders)
		CFRelease(mResponseHeaders), mResponseHeaders = NULL;

	mOffset = -1;

	return true;
}

SInt64 HTTPInputSource::Read(void *buffer, SInt64 byteCount)
{
	CFStreamStatus status = CFReadStreamGetStatus(mReadStream);
		
	if(kCFStreamStatusAtEnd == status)
		return 0;
	else if(kCFStreamStatusNotOpen == status || kCFStreamStatusClosed == status || kCFStreamStatusError == status)
		return -1;

	CFIndex bytesRead = CFReadStreamRead(mReadStream, static_cast<UInt8 *>(buffer), byteCount);

	mOffset += bytesRead;

	return bytesRead;
}

SInt64 HTTPInputSource::GetLength()
{
	if(!mResponseHeaders)
		return -1;

	SInt64 contentLength = -1;
	
	CFStringRef contentLengthString = reinterpret_cast<CFStringRef>(CFDictionaryGetValue(mResponseHeaders, CFSTR("Content-Length")));
	if(contentLengthString)
		contentLength = CFStringGetIntValue(contentLengthString);
	
	return contentLength;
}

void HTTPInputSource::HandleNetworkEvent(CFReadStreamRef stream, CFStreamEventType type)
{
	switch(type) {
		case kCFStreamEventOpenCompleted:
			puts("kCFStreamEventOpenCompleted");
			mOffset = 0;
			break;

		case kCFStreamEventHasBytesAvailable:
			puts("kCFStreamEventHasBytesAvailable");
			if(NULL == mResponseHeaders) {
				CFTypeRef responseHeader = CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader);
				if(responseHeader)
					mResponseHeaders = CFHTTPMessageCopyAllHeaderFields(static_cast<CFHTTPMessageRef>(const_cast<void *>(responseHeader)));
				if(mResponseHeaders)CFShow(mResponseHeaders);
			}
			break;
		
		case kCFStreamEventErrorOccurred:
			puts("kCFStreamEventErrorOccurred");
			CFShow(CFReadStreamCopyError(stream));
			break;

		case kCFStreamEventEndEncountered:
			puts("kCFStreamEventEndEncountered");
			mEOSReached = true;
			break;
	}
}
