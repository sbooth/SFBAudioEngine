/*
 *  Copyright (C) 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include "AudioMetadata.h"
#include "SetTagFromMetadata.h"
#include "TagLibStringUtilities.h"

bool
SetTagFromMetadata(const AudioMetadata& metadata, TagLib::Tag *tag)
{
	if(nullptr == tag)
		return false;

	tag->setTitle(TagLib::StringFromCFString(metadata.GetTitle()));
	tag->setArtist(TagLib::StringFromCFString(metadata.GetArtist()));
	tag->setAlbum(TagLib::StringFromCFString(metadata.GetAlbumTitle()));
	tag->setComment(TagLib::StringFromCFString(metadata.GetComment()));
	tag->setGenre(TagLib::StringFromCFString(metadata.GetGenre()));
	tag->setYear(metadata.GetReleaseDate() ? CFStringGetIntValue(metadata.GetReleaseDate()) : 0);

	int track = 0;
	if(metadata.GetTrackNumber())
		// Ignore return value
		CFNumberGetValue(metadata.GetTrackNumber(), kCFNumberIntType, &track);
	tag->setTrack(track);

	return true;
}
