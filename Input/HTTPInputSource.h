/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

#if TARGET_OS_IPHONE
# include <CFNetwork/CFNetwork.h>
#else
# include <CoreServices/CoreServices.h>
#endif

#include "InputSource.h"

namespace SFB {

	class HTTPInputSource : public InputSource
	{

	public:

		// Creation
		explicit HTTPInputSource(CFURLRef url);

	private:

		// Bytestream access
		virtual bool _Open(CFErrorRef *error);
		virtual bool _Close(CFErrorRef *error);

		// Functionality
		virtual SInt64 _Read(void *buffer, SInt64 byteCount);
		inline virtual bool _AtEOF()	const					{ return mEOSReached; }

		inline virtual SInt64 _GetOffset() const				{ return mOffset; }
		virtual SInt64 _GetLength() const;

		// Seeking support
		inline virtual bool _SupportsSeeking() const			{ return true; }
		virtual bool _SeekToOffset(SInt64 offset);

		CFStringRef CopyContentMIMEType() const;

		// Data members
		SFB::CFHTTPMessage				mRequest;
		SFB::CFReadStream				mReadStream;
		SFB::CFDictionary				mResponseHeaders;
		bool							mEOSReached;
		SInt64							mOffset;
		SInt64							mDesiredOffset;

	public:

		// Callbacks- for internal use only
		void HandleNetworkEvent(CFReadStreamRef stream, CFStreamEventType type);
	};

}
