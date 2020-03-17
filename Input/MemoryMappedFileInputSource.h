/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <functional>
#include <memory>

#include <sys/stat.h>

#include "InputSource.h"

namespace SFB {

	// ========================================
	// InputSource serving bytes from a memory-mapped file
	// ========================================
	class MemoryMappedFileInputSource : public InputSource
	{

	public:

		// Creation
		explicit MemoryMappedFileInputSource(CFURLRef url);

	private:

		// Bytestream access
		virtual bool _Open(CFErrorRef *error);
		virtual bool _Close(CFErrorRef *error);

		// Functionality
		virtual SInt64 _Read(void *buffer, SInt64 byteCount);
		virtual bool _AtEOF() const								{ return ((mCurrentPosition - mMemory.get()) == mFilestats.st_size); }

		inline virtual SInt64 _GetOffset() const				{ return (mCurrentPosition - mMemory.get()); }
		inline virtual SInt64 _GetLength() const				{ return mFilestats.st_size; }

		// Seeking support
		inline virtual bool _SupportsSeeking() const			{ return true; }
		virtual bool _SeekToOffset(SInt64 offset);

		using unique_mappedmem_ptr = std::unique_ptr<int8_t, std::function<int(int8_t *)>>;

		// Data members
		struct stat						mFilestats;
		unique_mappedmem_ptr			mMemory;
		int8_t							*mCurrentPosition;
	};

}
