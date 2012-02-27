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
#include "SetAPETagFromMetadata.h"
#include "TagLibStringUtilities.h"
#include "Logger.h"

// ========================================
// APE tag utilities
// ========================================
static bool
SetAPETag(TagLib::APE::Tag *tag, const char *key, CFStringRef value)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	// Remove the existing comment with this name
	tag->removeItem(key);
	
	// Nothing left to do if value is nullptr
	if(nullptr == value)
		return true;
	
	tag->addValue(key, TagLib::StringFromCFString(value));
	
	return true;
}

static bool
SetAPETagNumber(TagLib::APE::Tag *tag, const char *key, CFNumberRef value)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	CFStringRef numberString = nullptr;
	
	if(nullptr != value)
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@"), value);
	
	bool result = SetAPETag(tag, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = nullptr;
	
	return result;
}

static bool
SetAPETagBoolean(TagLib::APE::Tag *tag, const char *key, CFBooleanRef value)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	if(nullptr == value)
		return SetAPETag(tag, key, nullptr);
	else if(CFBooleanGetValue(value))
		return SetAPETag(tag, key, CFSTR("1"));
	else
		return SetAPETag(tag, key, CFSTR("0"));
}

static bool
SetAPETagDouble(TagLib::APE::Tag *tag, const char *key, CFNumberRef value, CFStringRef format = nullptr)
{
	assert(nullptr != tag);
	assert(nullptr != key);
	
	CFStringRef numberString = nullptr;
	
	if(nullptr != value) {
		double f;
		if(!CFNumberGetValue(value, kCFNumberDoubleType, &f)) {
			LOGGER_ERR("org.sbooth.AudioEngine", "CFNumberGetValue failed");
			return false;
		}
		
		numberString = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, nullptr == format ? CFSTR("%f") : format, f);
	}
	
	bool result = SetAPETag(tag, key, numberString);
	
	if(numberString)
		CFRelease(numberString), numberString = nullptr;
	
	return result;
}

bool
SetAPETagFromMetadata(const AudioMetadata& metadata, TagLib::APE::Tag *tag)
{
	if(nullptr == tag)
		return false;

	// Standard tags
	SetAPETag(tag, "ALBUM", metadata.GetAlbumTitle());
	SetAPETag(tag, "ARTIST", metadata.GetArtist());
	SetAPETag(tag, "ALBUMARTIST", metadata.GetAlbumArtist());
	SetAPETag(tag, "COMPOSER", metadata.GetComposer());
	SetAPETag(tag, "GENRE", metadata.GetGenre());
	SetAPETag(tag, "DATE", metadata.GetReleaseDate());
	SetAPETag(tag, "DESCRIPTION", metadata.GetComment());
	SetAPETag(tag, "TITLE", metadata.GetTitle());
	SetAPETagNumber(tag, "TRACKNUMBER", metadata.GetTrackNumber());
	SetAPETagNumber(tag, "TRACKTOTAL", metadata.GetTrackTotal());
	SetAPETagBoolean(tag, "COMPILATION", metadata.GetCompilation());
	SetAPETagNumber(tag, "DISCNUMBER", metadata.GetDiscNumber());
	SetAPETagNumber(tag, "DISCTOTAL", metadata.GetDiscTotal());
	SetAPETagNumber(tag, "BPM", metadata.GetBPM());
	SetAPETagNumber(tag, "RATING", metadata.GetRating());
	SetAPETag(tag, "ISRC", metadata.GetISRC());
	SetAPETag(tag, "MCN", metadata.GetMCN());
	
	// Additional metadata
	CFDictionaryRef additionalMetadata = metadata.GetAdditionalMetadata();
	if(nullptr != additionalMetadata) {
		CFIndex count = CFDictionaryGetCount(additionalMetadata);
		
		const void * keys [count];
		const void * values [count];
		
		CFDictionaryGetKeysAndValues(additionalMetadata, reinterpret_cast<const void **>(keys), reinterpret_cast<const void **>(values));
		
		for(CFIndex i = 0; i < count; ++i) {
			CFIndex keySize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(reinterpret_cast<CFStringRef>(keys[i])), kCFStringEncodingASCII);
			char key [keySize + 1];
			
			if(!CFStringGetCString(reinterpret_cast<CFStringRef>(keys[i]), key, keySize + 1, kCFStringEncodingASCII)) {
				LOGGER_ERR("org.sbooth.AudioEngine", "CFStringGetCString failed");
				continue;
			}
			
			SetAPETag(tag, key, reinterpret_cast<CFStringRef>(values[i]));
		}
	}
	
	// ReplayGain info
	SetAPETagDouble(tag, "REPLAYGAIN_REFERENCE_LOUDNESS", metadata.GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));
	SetAPETagDouble(tag, "REPLAYGAIN_TRACK_GAIN", metadata.GetReplayGainReferenceLoudness(), CFSTR("%+2.2f dB"));
	SetAPETagDouble(tag, "REPLAYGAIN_TRACK_PEAK", metadata.GetReplayGainTrackGain(), CFSTR("%1.8f"));
	SetAPETagDouble(tag, "REPLAYGAIN_ALBUM_GAIN", metadata.GetReplayGainAlbumGain(), CFSTR("%+2.2f dB"));
	SetAPETagDouble(tag, "REPLAYGAIN_ALBUM_PEAK", metadata.GetReplayGainAlbumPeak(), CFSTR("%1.8f"));

	return true;
}
