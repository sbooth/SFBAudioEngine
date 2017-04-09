/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <memory>

#include "InputSource.h"

namespace SFB {

	// ========================================
	// InputSource serving bytes from a region of memory
	// ========================================
	class MemoryInputSource : public InputSource
	{

	public:

		// Creation
		MemoryInputSource(const void *bytes, SInt64 byteCount, bool copyBytes = true);

	private:

		// Bytestream access
		virtual bool _Open(CFErrorRef *error);
		virtual bool _Close(CFErrorRef *error);

		// Functionality
		virtual SInt64 _Read(void *buffer, SInt64 byteCount);
		virtual bool _AtEOF() const								{ return ((mCurrentPosition - mMemory.get()) == mByteCount); }

		inline virtual SInt64 _GetOffset() const				{ return (mCurrentPosition - mMemory.get()); }
		inline virtual SInt64 _GetLength() const				{ return mByteCount; }

		// Seeking support
		inline virtual bool _SupportsSeeking() const			{ return true; }
		virtual bool _SeekToOffset(SInt64 offset);

		using unique_mem_ptr = std::unique_ptr<int8_t, void (*)(int8_t *)>;

		// Data members
		SInt64							mByteCount;
		unique_mem_ptr					mMemory;
		const int8_t					*mCurrentPosition;
	};

}
