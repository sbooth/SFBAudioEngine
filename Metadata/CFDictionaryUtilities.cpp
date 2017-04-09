/*
 * Copyright (c) 2012 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "CFDictionaryUtilities.h"
#include "CFWrapper.h"

void SFB::AddIntToDictionary(CFMutableDictionaryRef d, CFStringRef key, int value)
{
	if(nullptr == d || nullptr == key)
		return;

	SFB::CFNumber num(kCFNumberIntType, &value);
	if(num)
		CFDictionarySetValue(d, key, num);
}

void SFB::AddIntToDictionaryAsString(CFMutableDictionaryRef d, CFStringRef key, int value)
{
	if(nullptr == d || nullptr == key)
		return;

	SFB::CFString str(nullptr, CFSTR("%d"), value);
	if(str)
		CFDictionarySetValue(d, key, str);
}

void SFB::AddLongLongToDictionary(CFMutableDictionaryRef d, CFStringRef key, long long value)
{
	if(nullptr == d || nullptr == key)
		return;

	SFB::CFNumber num(kCFNumberLongLongType, &value);
	if(num)
		CFDictionarySetValue(d, key, num);
}

void SFB::AddFloatToDictionary(CFMutableDictionaryRef d, CFStringRef key, float value)
{
	if(nullptr == d || nullptr == key)
		return;

	SFB::CFNumber num(kCFNumberFloatType, &value);
	if(num)
		CFDictionarySetValue(d, key, num);
}

void SFB::AddDoubleToDictionary(CFMutableDictionaryRef d, CFStringRef key, double value)
{
	if(nullptr == d || nullptr == key)
		return;

	SFB::CFNumber num(kCFNumberDoubleType, &value);
	if(num)
		CFDictionarySetValue(d, key, num);
}
