/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "SetTagFromMetadata.h"
#include "TagLibStringUtilities.h"

void SFB::Audio::SetTagFromMetadata(SFBAudioMetadata *metadata, TagLib::Tag *tag)
{
	NSCParameterAssert(metadata != nil);
	assert(nullptr != tag);

	tag->setTitle(TagLib::StringFromNSString(metadata.title));
	tag->setArtist(TagLib::StringFromNSString(metadata.artist));
	tag->setAlbum(TagLib::StringFromNSString(metadata.albumTitle));
	tag->setComment(TagLib::StringFromNSString(metadata.comment));
	tag->setGenre(TagLib::StringFromNSString(metadata.genre));
	tag->setYear(metadata.releaseDate ? (unsigned int)metadata.releaseDate.intValue : 0);
	tag->setTrack(metadata.trackNumber.unsignedIntValue);
}
