/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <memory>

#include <sys/stat.h>

#include "InputSource.h"

namespace SFB {

	// ========================================
	// InputSource serving bytes from a file fully loaded in RAM
	// ========================================
	class InMemoryFileInputSource : public InputSource
	{

	public:

		// Creation
		explicit InMemoryFileInputSource(CFURLRef url);

	private:

		// Bytestream access
		virtual bool _Open(CFErrorRef *error);
		virtual bool _Close(CFErrorRef *error);

		inline virtual bool _IsOpen() const						{ return (nullptr != mMemory);}

		// Functionality
		virtual SInt64 _Read(void *buffer, SInt64 byteCount);
		inline virtual bool _AtEOF() const						{ return ((mCurrentPosition - mMemory.get()) == mFilestats.st_size); }

		inline virtual SInt64 _GetOffset() const				{ return (mCurrentPosition - mMemory.get()); }
		inline virtual SInt64 _GetLength() const				{ return mFilestats.st_size; }

		// Seeking support
		inline virtual bool _SupportsSeeking() const			{ return true; }
		virtual bool _SeekToOffset(SInt64 offset);

		// Data members
		struct stat						mFilestats;
		std::unique_ptr<int8_t []>		mMemory;
		int8_t							*mCurrentPosition;
	};

}
