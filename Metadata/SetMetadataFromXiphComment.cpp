/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include "AudioEngineDefines.h"
#include "AudioMetadata.h"

#include "SetMetadataFromXiphComment.h"

bool
SetMetadataFromXiphComment(AudioMetadata *metadata, TagLib::Ogg::XiphComment *tag)
{
	assert(NULL != metadata);
	assert(NULL != tag);
	
	CFMutableDictionaryRef additionalMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 
																		  32,
																		  &kCFTypeDictionaryKeyCallBacks,
																		  &kCFTypeDictionaryValueCallBacks);
	
	// A map <String, StringList>
	TagLib::Ogg::FieldListMap fieldList = tag->fieldListMap();
	
	for(TagLib::Ogg::FieldListMap::ConstIterator it = fieldList.begin(); it != fieldList.end(); ++it) {
		CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault,
													it->first.toCString(true),
													kCFStringEncodingUTF8);
		
		// Vorbis allows multiple comments with the same key, but this isn't supported by AudioMetadata
		CFStringRef value = CFStringCreateWithCString(kCFAllocatorDefault,
													  it->second.front().toCString(true),
													  kCFStringEncodingUTF8);
		
		if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUM"), kCFCompareCaseInsensitive))
			metadata->SetAlbumTitle(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTIST"), kCFCompareCaseInsensitive))
			metadata->SetArtist(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTIST"), kCFCompareCaseInsensitive))
			metadata->SetAlbumArtist(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSER"), kCFCompareCaseInsensitive))
			metadata->SetComposer(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GENRE"), kCFCompareCaseInsensitive))
			metadata->SetGenre(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DATE"), kCFCompareCaseInsensitive))
			metadata->SetReleaseDate(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DESCRIPTION"), kCFCompareCaseInsensitive))
			metadata->SetComment(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLE"), kCFCompareCaseInsensitive))
			metadata->SetTitle(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKNUMBER"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			metadata->SetTrackNumber(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKTOTAL"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			metadata->SetTrackTotal(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPILATION"), kCFCompareCaseInsensitive))
			metadata->SetCompilation(CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCNUMBER"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			metadata->SetDiscNumber(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCTOTAL"), kCFCompareCaseInsensitive)) {
			int num = CFStringGetIntValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
			metadata->SetDiscTotal(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ISRC"), kCFCompareCaseInsensitive))
			metadata->SetISRC(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MCN"), kCFCompareCaseInsensitive))
			metadata->SetMCN(value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_REFERENCE_LOUDNESS"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			metadata->SetReplayGainReferenceLoudness(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_GAIN"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			metadata->SetReplayGainTrackGain(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_PEAK"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			metadata->SetReplayGainTrackPeak(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_GAIN"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			metadata->SetReplayGainAlbumGain(number);
			CFRelease(number), number = NULL;
		}
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_PEAK"), kCFCompareCaseInsensitive)) {
			double num = CFStringGetDoubleValue(value);
			CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
			metadata->SetReplayGainAlbumPeak(number);
			CFRelease(number), number = NULL;
		}
		// Put all unknown tags into the additional metadata
		else
			CFDictionarySetValue(additionalMetadata, key, value);
		
		CFRelease(key), key = NULL;
		CFRelease(value), value = NULL;
	}
	
	if(CFDictionaryGetCount(additionalMetadata))
		metadata->SetAdditionalMetadata(additionalMetadata);
	
	CFRelease(additionalMetadata), additionalMetadata = NULL;
	
	return true;
}
