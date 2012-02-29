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

// ========================================
// Error Codes
// ========================================
extern const CFStringRef		InputSourceErrorDomain;

enum {
	InputSourceFileNotFoundError			= 0,
	InputSourceInputOutputError				= 1
};

// ========================================
// Flags
// ========================================
enum {
	InputSourceFlagMemoryMapFiles			= 1 << 0,
	InputSourceFlagLoadFilesInMemory		= 1 << 1
};

// ========================================
// An abstract class presenting access to a stream of bytes
// ========================================
class InputSource
{

public:

	// ========================================
	// Factory methods that return an InputSource for the specified URL, or nullptr on failure
	static InputSource * CreateInputSourceForURL(CFURLRef url, int flags = 0, CFErrorRef *error = nullptr);

	// ========================================
	// Destruction
	virtual ~InputSource();

	// This class is non-copyable
	InputSource(const InputSource& rhs) = delete;
	InputSource& operator=(const InputSource& rhs) = delete;

	// ========================================
	// The URL this source will process
	inline CFURLRef GetURL() const							{ return mURL; }
	
	// ========================================
	// Bytestream access (must be implemented by subclasses)
	virtual bool Open(CFErrorRef *error = nullptr) = 0;
	virtual bool Close(CFErrorRef *error = nullptr) = 0;
	
	inline bool IsOpen() const								{ return mIsOpen; }

	// ========================================
	// Returns the number of bytes actually read
	virtual SInt64 Read(void *buffer, SInt64 byteCount) = 0;
	virtual bool AtEOF() const = 0;
	
	virtual SInt64 GetOffset() const = 0;
	virtual SInt64 GetLength() const = 0;

	// ========================================
	// Seeking support (optional)
	virtual bool SupportsSeeking() const					{ return false; }
	virtual bool SeekToOffset(SInt64 /*offset*/)			{ return false; }
	
protected:
	
	CFURLRef						mURL;				// The location of the bytes to be read
	bool							mIsOpen;			// Subclasses should set this to true if Open() is successful

	// ========================================
	// For subclass use only
	InputSource();
	InputSource(CFURLRef url);

};
