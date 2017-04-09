/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "SetTagFromMetadata.h"
#include "AudioMetadata.h"
#include "TagLibStringUtilities.h"

bool SFB::Audio::SetTagFromMetadata(const Metadata& metadata, TagLib::Tag *tag)
{
	if(nullptr == tag)
		return false;

	tag->setTitle(TagLib::StringFromCFString(metadata.GetTitle()));
	tag->setArtist(TagLib::StringFromCFString(metadata.GetArtist()));
	tag->setAlbum(TagLib::StringFromCFString(metadata.GetAlbumTitle()));
	tag->setComment(TagLib::StringFromCFString(metadata.GetComment()));
	tag->setGenre(TagLib::StringFromCFString(metadata.GetGenre()));
	tag->setYear(metadata.GetReleaseDate() ? (unsigned int)CFStringGetIntValue(metadata.GetReleaseDate()) : 0);

	int track = 0;
	if(metadata.GetTrackNumber())
		// Ignore return value
		CFNumberGetValue(metadata.GetTrackNumber(), kCFNumberIntType, &track);
	tag->setTrack((unsigned int)track);

	return true;
}
