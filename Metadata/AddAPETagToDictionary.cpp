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

#include <taglib/flacpicture.h>

#include "AddAPETagToDictionary.h"
#include "AudioMetadata.h"
#include "Base64Utilities.h"
#include "CFDictionaryUtilities.h"

bool
AddAPETagToDictionary(CFMutableDictionaryRef dictionary, std::vector<AttachedPicture *>& attachedPictures, const TagLib::APE::Tag *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	if(tag->isEmpty())
		return true;

	CFMutableDictionaryRef additionalMetadata = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	for(auto iterator : tag->itemListMap()) {
		auto item = iterator.second;

		if(item.isEmpty())
			continue;

		if(TagLib::APE::Item::Text == item.type()) {
			CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, item.key().toCString(true), kCFStringEncodingUTF8);
			CFStringRef value = CFStringCreateWithCString(kCFAllocatorDefault, item.toString().toCString(true), kCFStringEncodingUTF8);

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
#if 0
			else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("METADATA_BLOCK_PICTURE"), kCFCompareCaseInsensitive)) {
				// Handle embedded pictures
				for(auto blockIterator : item.values()) {
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
#endif
			// Put all unknown tags into the additional metadata
			else
				CFDictionarySetValue(additionalMetadata, key, value);

			CFRelease(key), key = nullptr;
			CFRelease(value), value = nullptr;
		}
		else if(TagLib::APE::Item::Binary == item.type()) {
			CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, item.key().toCString(true), kCFStringEncodingUTF8);

			// From http://www.hydrogenaudio.org/forums/index.php?showtopic=40603&view=findpost&p=504669
			/*
			 <length> 32 bit
			 <flags with binary bit set> 32 bit
			 <field name> "Cover Art (Front)"|"Cover Art (Back)"
			 0x00
			 <description> UTF-8 string (needs to be a file name to be recognized by AudioShell - meh)
			 0x00
			 <cover data> binary
			 */
			if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("Cover Art (Front)"), kCFCompareCaseInsensitive) || kCFCompareEqualTo == CFStringCompare(key, CFSTR("Cover Art (Back)"), kCFCompareCaseInsensitive)) {
				int pos = item.binaryData().find('\0');
				if(-1 != pos && 3 < item.value().size()) {
					CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(item.value().mid(pos + 1).data()), item.value().size() - pos - 1);
					CFStringRef description = CFStringCreateWithCString(kCFAllocatorDefault, TagLib::String(item.value().mid(0, pos), TagLib::String::UTF8).toCString(true), kCFStringEncodingUTF8);

					AttachedPicture *p = new AttachedPicture(data, kCFCompareEqualTo == CFStringCompare(key, CFSTR("Cover Art (Front)"), kCFCompareCaseInsensitive) ? AttachedPicture::Type::FrontCover : AttachedPicture::Type::BackCover, description);
					attachedPictures.push_back(p);

					if(data)
						CFRelease(data), data = nullptr;

					if(description)
						CFRelease(description), description = nullptr;
				}
			}

			CFRelease(key), key = nullptr;
		}
	}

	if(CFDictionaryGetCount(additionalMetadata))
		CFDictionarySetValue(dictionary, kMetadataAdditionalMetadataKey, additionalMetadata);
	
	CFRelease(additionalMetadata), additionalMetadata = nullptr;

	return true;
}
