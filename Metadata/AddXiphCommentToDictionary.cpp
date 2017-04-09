/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/flacpicture.h>

#include "AddXiphCommentToDictionary.h"
#include "AudioMetadata.h"
#include "CFWrapper.h"
#include "Base64Utilities.h"
#include "CFDictionaryUtilities.h"

bool SFB::Audio::AddXiphCommentToDictionary(CFMutableDictionaryRef dictionary, std::vector<std::shared_ptr<AttachedPicture>>& attachedPictures, const TagLib::Ogg::XiphComment *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	SFB::CFMutableDictionary additionalMetadata(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for(auto it : tag->fieldListMap()) {
		// According to the Xiph comment specification keys should only contain a limited subset of ASCII, but UTF-8 is a safer choice
		SFB::CFString key(it.first.toCString(true), kCFStringEncodingUTF8);

		// Vorbis allows multiple comments with the same key, but this isn't supported by AudioMetadata
		SFB::CFString value(it.second.front().toCString(true), kCFStringEncodingUTF8);

		if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUM"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kAlbumTitleKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTIST"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kArtistKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTIST"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kAlbumArtistKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSER"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kComposerKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GENRE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kGenreKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DATE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kReleaseDateKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DESCRIPTION"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kCommentKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLE"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kTitleKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKNUMBER"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, Metadata::kTrackNumberKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TRACKTOTAL"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, Metadata::kTrackTotalKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPILATION"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kCompilationKey, CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCNUMBER"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, Metadata::kDiscNumberKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("DISCTOTAL"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, Metadata::kDiscTotalKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("LYRICS"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kLyricsKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("BPM"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, Metadata::kBPMKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("RATING"), kCFCompareCaseInsensitive))
			AddIntToDictionary(dictionary, Metadata::kRatingKey, CFStringGetIntValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ISRC"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kISRCKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MCN"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kMCNKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MUSICBRAINZ_ALBUMID"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kMusicBrainzReleaseIDKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("MUSICBRAINZ_TRACKID"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kMusicBrainzRecordingIDKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("TITLESORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kTitleSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMTITLESORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kAlbumTitleSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ARTISTSORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kArtistSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("ALBUMARTISTSORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kAlbumArtistSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("COMPOSERSORT"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kComposerSortOrderKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("GROUPING"), kCFCompareCaseInsensitive))
			CFDictionarySetValue(dictionary, Metadata::kGroupingKey, value);
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_REFERENCE_LOUDNESS"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, Metadata::kReferenceLoudnessKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_GAIN"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, Metadata::kTrackGainKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_TRACK_PEAK"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, Metadata::kTrackPeakKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_GAIN"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, Metadata::kAlbumGainKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("REPLAYGAIN_ALBUM_PEAK"), kCFCompareCaseInsensitive))
			AddDoubleToDictionary(dictionary, Metadata::kAlbumPeakKey, CFStringGetDoubleValue(value));
		else if(kCFCompareEqualTo == CFStringCompare(key, CFSTR("METADATA_BLOCK_PICTURE"), kCFCompareCaseInsensitive)) {
			// Handle embedded pictures
			for(auto blockIterator : it.second) {
				auto encodedBlock = blockIterator.data(TagLib::String::UTF8);

				// Decode the Base-64 encoded data
				auto decodedBlock = TagLib::DecodeBase64(encodedBlock);

				// Create the picture
				TagLib::FLAC::Picture picture;
				picture.parse(decodedBlock);

				SFB::CFData data((const UInt8 *)picture.data().data(), (CFIndex)picture.data().size());

				SFB::CFString description;
				if(!picture.description().isEmpty())
					description = SFB::CFString(picture.description().toCString(true), kCFStringEncodingUTF8);

				attachedPictures.push_back(std::make_shared<AttachedPicture>(data, (AttachedPicture::Type)picture.type(), description));
			}
		}
		// Put all unknown tags into the additional metadata
		else
			CFDictionarySetValue(additionalMetadata, key, value);
	}

	if(CFDictionaryGetCount(additionalMetadata))
		CFDictionarySetValue(dictionary, Metadata::kAdditionalMetadataKey, additionalMetadata);

	return true;
}
