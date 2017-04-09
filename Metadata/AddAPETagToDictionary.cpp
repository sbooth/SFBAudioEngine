/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/flacpicture.h>

#include "AddAPETagToDictionary.h"
#include "AudioMetadata.h"
#include "CFWrapper.h"
#include "Base64Utilities.h"
#include "CFDictionaryUtilities.h"

bool SFB::Audio::AddAPETagToDictionary(CFMutableDictionaryRef dictionary, std::vector<std::shared_ptr<AttachedPicture>>& attachedPictures, const TagLib::APE::Tag *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	if(tag->isEmpty())
		return true;

	SFB::CFMutableDictionary additionalMetadata(0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for(auto iterator : tag->itemListMap()) {
		auto item = iterator.second;

		if(item.isEmpty())
			continue;

		if(TagLib::APE::Item::Text == item.type()) {
			SFB::CFString key(item.key().toCString(true), kCFStringEncodingUTF8);
			SFB::CFString value(item.toString().toCString(true), kCFStringEncodingUTF8);

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

					SFB::CFData data((const UInt8 *)picture.data().data(), picture.data().size());

					SFB::CFString description = nullptr;
					if(!picture.description().isNull())
						description(picture.description().toCString(true), kCFStringEncodingUTF8);

					attachedPictures.push_back(std::make_shared<AttachedPicture>(data, (AttachedPicture::Type)picture.type(), description));
				}
			}
#endif
			// Put all unknown tags into the additional metadata
			else
				CFDictionarySetValue(additionalMetadata, key, value);
		}
		else if(TagLib::APE::Item::Binary == item.type()) {
			SFB::CFString key(item.key().toCString(true), kCFStringEncodingUTF8);

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
				auto binaryData = item.binaryData();
				size_t pos = binaryData.find('\0');
				if(TagLib::ByteVector::npos() != pos && 3 < binaryData.size()) {
					SFB::CFData data((const UInt8 *)binaryData.mid(pos + 1).data(), (CFIndex)(binaryData.size() - pos - 1));
					SFB::CFString description(TagLib::String(binaryData.mid(0, pos), TagLib::String::UTF8).toCString(true), kCFStringEncodingUTF8);

					attachedPictures.push_back(std::make_shared<AttachedPicture>(data, kCFCompareEqualTo == CFStringCompare(key, CFSTR("Cover Art (Front)"), kCFCompareCaseInsensitive) ? AttachedPicture::Type::FrontCover : AttachedPicture::Type::BackCover, description));
				}
			}
		}
	}

	if(CFDictionaryGetCount(additionalMetadata))
		CFDictionarySetValue(dictionary, Metadata::kAdditionalMetadataKey, additionalMetadata);

	return true;
}
