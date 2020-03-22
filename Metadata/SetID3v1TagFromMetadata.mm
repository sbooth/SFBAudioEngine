/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "SetID3v1TagFromMetadata.h"
#include "SetTagFromMetadata.h"

void SFB::Audio::SetID3v1TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v1::Tag *tag)
{
	// TagLib::ID3v1::Tag has no additonal functionality over TagLib::Tag
	SetTagFromMetadata(metadata, tag);
}
