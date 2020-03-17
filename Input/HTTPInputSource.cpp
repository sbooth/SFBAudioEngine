/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "HTTPInputSource.h"

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
		SFB::CFString byteRange(nullptr, CFSTR("bytes=%lld-"), mDesiredOffset);
		CFHTTPMessageSetHeaderFieldValue(mRequest, CFSTR("Range"), byteRange);
	}

	mReadStream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, mRequest);
	if(!mReadStream) {
		if(error)
			*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, ENOMEM, nullptr);
		return false;
	}

    if(CFStringHasPrefix(CFURLGetString(GetURL()), CFSTR("https://"))) {
        CFReadStreamSetProperty(mReadStream, kCFStreamPropertySocketSecurityLevel, kCFStreamSocketSecurityLevelNegotiatedSSL);
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
				SFB::CFType responseHeader(CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader));
				if(responseHeader) {
					mResponseHeaders = CFHTTPMessageCopyAllHeaderFields((CFHTTPMessageRef)responseHeader.Object());
				}
			}
			break;

		case kCFStreamEventErrorOccurred:
		{
			SFB::CFError error(CFReadStreamCopyError(stream));
			if(error)
				os_log_error(OS_LOG_DEFAULT, "Error: %{public}@", error.Object());
			break;
		}

		case kCFStreamEventEndEncountered:
			mEOSReached = true;
			break;
	}
}
