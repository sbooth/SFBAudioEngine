/*
 *  Copyright (C) 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <taglib/flacpicture.h>

#include "AddXiphCommentToDictionary.h"
#include "AudioMetadata.h"
#include "Base64Utilities.h"
#include "CFDictionaryUtilities.h"

bool
AddXiphCommentToDictionary(CFMutableDictionaryRef dictionary, std::vector<AttachedPicture *>& attachedPictures, const TagLib::Ogg::XiphComment *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	attachedPictures.clear();
	
	CFMutableDictionaryRef additionalMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	for(auto it : tag->fieldListMap()) {
		// According to the Xiph comment specification keys should only contain a limited subset of ASCII, but UTF-8 is a safer choice
		CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, it.first.toCString(true), kCFStringEncodingUTF8);
		
		// Vorbis allows multiple comments with the same key, but this isn't supported by AudioMetadata
		CFStringRef value = CFStringCreateWithCString(kCFAllocatorDefault, it.second.front().toCString(true), kCFStringEncodingUTF8);
		
		if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUM"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataAlbumTitleKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTIST"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataArtistKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTIST"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataAlbumArtistKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSER"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataComposerKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GENRE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataGenreKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DATE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataReleaseDateKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DESCRIPTION"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataCommentKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataTitleKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKNUMBER"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, kMetadataTrackNumberKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKTOTAL"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, kMetadataTrackTotalKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPILATION"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataCompilationKey, CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCNUMBER"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, kMetadataDiscNumberKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCTOTAL"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, kMetadataDiscTotalKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("LYRICS"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataLyricsKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("BPM"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, kMetadataBPMKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("RATING"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, kMetadataRatingKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ISRC"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataISRCKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MCN"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataMCNKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLESORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataTitleSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMTITLESORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataAlbumTitleSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTISTSORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataArtistSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTISTSORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataAlbumArtistSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSERSORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataComposerSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GROUPING"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, kMetadataGroupingKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_REFERENCE_LOUDNESS"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, kReplayGainReferenceLoudnessKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_GAIN"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, kReplayGainTrackGainKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_PEAK"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, kReplayGainTrackPeakKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_GAIN"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, kReplayGainAlbumGainKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_PEAK"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, kReplayGainAlbumPeakKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("METADATA_BLOCK_PICTURE"), kCFCompareCaseInsensitive)) {
			// Handle embedded pictures
			for(auto blockIterator : it.second) {
				auto encodedBlock = blockIterator.data(TagLib::String::UTF8);

				// Decode the Base-64 encoded data
				auto decodedBlock = TagLib::DecodeBase64(encodedBlock);

				// Create the picture
				TagLib::FLAC::Picture picture;
				picture.parse(decodedBlock);
				
				CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(picture.data().data()), picture.data().size());

				CFStringRef description = nullptr;
				if(!picture.description().isNull())
					description = CFStringCreateWithCString(kCFAllocatorDefault, picture.description().toCString(true), kCFStringEncodingUTF8);

				AttachedPicture *p = new AttachedPicture(data, static_cast<AttachedPicture::Type>(picture.type()), description);
				attachedPictures.push_back(p);
				
				if(data)
					CFRelease(data), data = nullptr;
				
				if(description)
					CFRelease(description), description = nullptr;
			}
		}
		// Put all unknown tags into the additional metadata
		else
			CFDictionarySetValue(additionalMetadata, key, value);
		
		CFRelease(key), key = nullptr;
		CFRelease(value), value = nullptr;
	}
	
	if(CFDictionaryGetCount(additionalMetadata))
		CFDictionarySetValue(dictionary, kMetadataAdditionalMetadataKey, additionalMetadata);
	
	CFRelease(additionalMetadata), additionalMetadata = nullptr;
	
	return true;
}
