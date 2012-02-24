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

#include <stdio.h>
#include <sys/stat.h>

#include "InputSource.h"

class FileInputSource : public InputSource
{

public:

	// ========================================
	// Creation
	FileInputSource(CFURLRef url);

	// ========================================
	// Destruction
	virtual ~FileInputSource();

	// ========================================
	// Bytestream access
	virtual bool Open(CFErrorRef *error = nullptr);
	virtual bool Close(CFErrorRef *error = nullptr);

	// ========================================
	//
	virtual SInt64 Read(void *buffer, SInt64 byteCount);
	virtual inline bool AtEOF() const						{ return feof(mFile); }
	
	virtual inline SInt64 GetOffset() const					{ return ftello(mFile); }
	virtual inline SInt64 GetLength() const					{ return mFilestats.st_size; }
	
	// ========================================
	// Seeking support
	virtual inline bool SupportsSeeking() const				{ return true; }
	virtual bool SeekToOffset(SInt64 offset);
	
private:
	
	struct stat						mFilestats;
	FILE							*mFile;

};
