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

#pragma once

#include <CoreFoundation/CoreFoundation.h>

#if TARGET_OS_IPHONE
# include <CFNetwork/CFNetwork.h>
#else
#include <CoreServices/CoreServices.h>
#endif
#include "InputSource.h"

class HTTPInputSource : public InputSource
{
	
public:
	
	// ========================================
	// Creation
	HTTPInputSource(CFURLRef url);
	
	// ========================================
	// Destruction
	virtual ~HTTPInputSource();
	
	// ========================================
	// Bytestream access
	virtual bool Open(CFErrorRef *error = nullptr);
	virtual bool Close(CFErrorRef *error = nullptr);
	
	// ========================================
	//
	virtual SInt64 Read(void *buffer, SInt64 byteCount);
	virtual inline bool AtEOF()	const						{ return mEOSReached; }
	
	virtual inline SInt64 GetOffset() const					{ return mOffset; }
	virtual SInt64 GetLength() const;
	
	// ========================================
	// Seeking support
	virtual inline bool SupportsSeeking() const				{ return true; }
	virtual bool SeekToOffset(SInt64 offset);

	// ========================================
	CFStringRef CopyContentMIMEType() const;

private:
	
	CFHTTPMessageRef				mRequest;
	CFReadStreamRef					mReadStream;
	CFDictionaryRef					mResponseHeaders;
	bool							mEOSReached;
	SInt64							mOffset;
	SInt64							mDesiredOffset;

public:
	
	// ========================================
	// Callbacks- for internal use only
	void HandleNetworkEvent(CFReadStreamRef stream, CFStreamEventType type);
};
