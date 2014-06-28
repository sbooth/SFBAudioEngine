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

#pragma once

#include <cstdio>
#include <memory>
#include <sys/stat.h>

#include "InputSource.h"

namespace SFB {

	class FileInputSource : public InputSource
	{

	public:

		// Creation
		FileInputSource(CFURLRef url);

	private:

		// Bytestream access
		virtual bool _Open(CFErrorRef *error);
		virtual bool _Close(CFErrorRef *error);

		// Functionality
		virtual SInt64 _Read(void *buffer, SInt64 byteCount)	{ return (SInt64)::fread(buffer, 1, (size_t)byteCount, mFile.get()); }
		inline virtual bool _AtEOF() const						{ return ::feof(mFile.get()); }

		inline virtual SInt64 _GetOffset() const				{ return ::ftello(mFile.get()); }
		inline virtual SInt64 _GetLength() const				{ return mFilestats.st_size; }

		// Seeking support
		inline virtual bool _SupportsSeeking() const			{ return true; }
		virtual bool _SeekToOffset(SInt64 offset)				{ return (0 == ::fseeko(mFile.get(), offset, SEEK_SET)); }

		using unique_FILE_ptr = std::unique_ptr<std::FILE, std::function<int(std::FILE *)>>;

		// Data members
		struct stat						mFilestats;
		unique_FILE_ptr					mFile;
	};

}
