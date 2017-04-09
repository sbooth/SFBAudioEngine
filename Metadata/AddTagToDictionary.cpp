/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "AddTagToDictionary.h"
#include "AudioMetadata.h"
#include "TagLibStringUtilities.h"
#include "CFDictionaryUtilities.h"

bool SFB::Audio::AddTagToDictionary(CFMutableDictionaryRef dictionary, const TagLib::Tag *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	TagLib::AddStringToCFDictionary(dictionary, Metadata::kTitleKey, tag->title());
	TagLib::AddStringToCFDictionary(dictionary, Metadata::kAlbumTitleKey, tag->album());
	TagLib::AddStringToCFDictionary(dictionary, Metadata::kArtistKey, tag->artist());
	TagLib::AddStringToCFDictionary(dictionary, Metadata::kGenreKey, tag->genre());

	if(tag->year())
		AddIntToDictionaryAsString(dictionary, Metadata::kReleaseDateKey, (int)tag->year());

	if(tag->track())
		AddIntToDictionary(dictionary, Metadata::kTrackNumberKey, (int)tag->track());

	TagLib::AddStringToCFDictionary(dictionary, Metadata::kCommentKey, tag->comment());

	return true;
}
