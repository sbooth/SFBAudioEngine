/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <cstdio>
#include <functional>
#include <memory>

#include <sys/stat.h>

#include "InputSource.h"

namespace SFB {

	class FileInputSource : public InputSource
	{

	public:

		// Creation
		explicit FileInputSource(CFURLRef url);

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
