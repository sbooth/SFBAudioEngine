/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "TagLibStringUtilities.h"
#include "CFWrapper.h"
#include "Logger.h"

TagLib::String TagLib::StringFromCFString(CFStringRef s)
{
	if(nullptr == s)
		return {};

	CFRange range = CFRangeMake(0, CFStringGetLength(s));
	CFIndex count;

	// Determine the length of the string in UTF-8
	CFStringGetBytes(s, range, kCFStringEncodingUTF8, 0, false, nullptr, 0, &count);

	std::vector<char> buf;
	buf.reserve((size_t)count + 1);

	// Convert it
	CFIndex used;
	CFIndex converted = CFStringGetBytes(s, range, kCFStringEncodingUTF8, 0, false, (UInt8 *)&buf[0], count, &used);

	if(CFStringGetLength(s) != converted)
		LOGGER_WARNING("org.sbooth.AudioEngine", "CFStringGetBytes failed: converted " << converted << " of " << CFStringGetLength(s) << " characters");

	// Add terminator
	buf[(size_t)used] = '\0';

	return {&buf[0], String::UTF8};
}

void TagLib::AddStringToCFDictionary(CFMutableDictionaryRef d, CFStringRef key, String value)
{
	if(nullptr == d || nullptr == key || value.isEmpty())
		return;

	SFB::CFString string(value.toCString(true), kCFStringEncodingUTF8);
	if(string)
		CFDictionarySetValue(d, key, string);
}
