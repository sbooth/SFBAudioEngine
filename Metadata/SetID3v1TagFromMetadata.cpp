/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "AudioMetadata.h"
#include "SetID3v1TagFromMetadata.h"
#include "SetTagFromMetadata.h"

bool SFB::Audio::SetID3v1TagFromMetadata(const Metadata& metadata, TagLib::ID3v1::Tag *tag)
{
	// TagLib::ID3v1::Tag has no additonal functionality over TagLib::Tag
	return SetTagFromMetadata(metadata, tag);
}
