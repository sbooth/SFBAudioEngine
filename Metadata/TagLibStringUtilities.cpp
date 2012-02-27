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

#include "TagLibStringUtilities.h"
#include "Logger.h"

TagLib::String 
TagLib::StringFromCFString(CFStringRef s)
{
	if(nullptr == s)
		return String::null;

	CFRange range = CFRangeMake(0, CFStringGetLength(s));
	CFIndex count;

	// Determine the length of the string in UTF-8
	CFStringGetBytes(s, range, kCFStringEncodingUTF8, 0, false, nullptr, 0, &count);

	char *buf = new char [count + 1];

	// Convert it
	CFIndex used;
	CFIndex converted = CFStringGetBytes(s, range, kCFStringEncodingUTF8, 0, false, reinterpret_cast<UInt8 *>(buf), count, &used);

	if(CFStringGetLength(s) != converted)
		LOGGER_WARNING("org.sbooth.AudioEngine", "CFStringGetBytes failed: converted " << converted << " of " << CFStringGetLength(s) << " characters");

	// Add terminator
	buf[used] = '\0';

	String result(buf, String::UTF8);

	delete [] buf;

	return result;
}

void TagLib::AddStringToCFDictionary(CFMutableDictionaryRef d, CFStringRef key, String value)
{
	if(nullptr == d || nullptr == key || value.isNull())
		return;
	
	CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, value.toCString(true), kCFStringEncodingUTF8);
	CFDictionarySetValue(d, key, string);
	CFRelease(string), string = nullptr;
}
