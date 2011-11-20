/*
 *  Copyright (C) 2011 Stephen F. Booth <me@sbooth.org>
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

#include "AddTagToDictionary.h"
#include "AudioMetadata.h"

bool
AddTagToDictionary(CFMutableDictionaryRef dictionary, const TagLib::Tag *tag)
{
	if(NULL == dictionary || NULL == tag)
		return false;

	if(!tag->title().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->title().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataTitleKey, str);
		CFRelease(str), str = NULL;
	}

	if(!tag->album().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->album().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataAlbumTitleKey, str);
		CFRelease(str), str = NULL;
	}

	if(!tag->artist().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->artist().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataArtistKey, str);
		CFRelease(str), str = NULL;
	}

	if(!tag->genre().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->genre().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataGenreKey, str);
		CFRelease(str), str = NULL;
	}

	if(tag->year()) {
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), tag->year());
		CFDictionarySetValue(dictionary, kMetadataReleaseDateKey, str);
		CFRelease(str), str = NULL;
	}

	if(tag->track()) {
		int trackNum = tag->track();
		CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackNum);
		CFDictionarySetValue(dictionary, kMetadataTrackNumberKey, num);
		CFRelease(num), num = NULL;
	}

	if(!tag->comment().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->comment().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataCommentKey, str);
		CFRelease(str), str = NULL;
	}

	return true;
}
