/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "AddID3v1TagToDictionary.h"
#include "AddTagToDictionary.h"

bool SFB::Audio::AddID3v1TagToDictionary(CFMutableDictionaryRef dictionary, const TagLib::ID3v1::Tag *tag)
{
	// ID3v1 tags are only supposed to contain characters in ISO 8859-1 format, but that isn't always the case
	// AddTagToDictionary assumes UTF-8, so everything should work properly
	// Currently TagLib::ID3v1::Tag doesn't implement any more functionality than TagLib::Tag
	return AddTagToDictionary(dictionary, tag);
}
