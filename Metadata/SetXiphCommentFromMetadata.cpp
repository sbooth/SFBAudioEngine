/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/flacpicture.h>
#include <ApplicationServices/ApplicationServices.h>

#include "SetXiphCommentFromMetadata.h"
#include "AudioMetadata.h"
#include "CFWrapper.h"
#include "TagLibStringUtilities.h"
#include "Base64Utilities.h"
#include "Logger.h"

// ========================================
// Xiph comment utilities
namespace {

	bool SetXiphComment(TagLib::Ogg::XiphComment *tag, const char *key, CFStringRef value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing comment with this name
		tag->removeFields(key);

		// Nothing left to do if value is nullptr
		if(nullptr == value)
			return true;

		tag->addField(key, TagLib::StringFromCFString(value));

		return true;
	}

	bool SetXiphCommentNumber(TagLib::Ogg::XiphComment *tag, const char *key, CFNumberRef value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		SFB::CFString numberString;
		if(nullptr != value)
			numberString = SFB::CFString(nullptr, CFSTR("%@"), value);

		bool result = SetXiphComment(tag, key, numberString);

		return result;
	}

	bool SetXiphCommentBoolean(TagLib::Ogg::XiphComment *tag, const char *key, CFBooleanRef value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		if(nullptr == value)
			return SetXiphComment(tag, key, nullptr);
		else if(CFBooleanGetValue(value))
			return SetXiphComment(tag, key, CFSTR("1"));
		else
			return SetXiphComment(tag, key, CFSTR("0"));
	}

	bool SetXiphCommentDouble(TagLib::Ogg::XiphComment *tag, const char *key, CFNumberRef value, CFStringRef format = nullptr)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		SFB::CFString numberString;
		if(nullptr != value) {
			double f;
			if(!CFNumberGetValue(value, kCFNumberDoubleType, &f))
				LOGGER_INFO("org.sbooth.AudioEngine", "CFNumberGetValue returned an approximation");

			numberString = SFB::CFString(nullptr, format ?: CFSTR("%f"), f);
		}

		bool result = SetXiphComment(tag, key, numberString);

		return result;
	}

}

bool SFB::Audio::SetXiphCommentFromMetadata(const Metadata& metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt)
{
	if(nullptr == tag)
		return false;

	// Standard tags
	SetXiphComment(tag, "ALBUM", metadata.GetAlbumTitle());
	SetXiphComment(tag, "ARTIST", metadata.GetArtist());
	SetXiphComment(tag, "ALBUMARTIST", metadata.GetAlbumArtist());
	SetXiphComment(tag, "COMPOSER", metadata.GetComposer());
	SetXiphComment(tag, "GENRE", metadata.GetGenre());
	SetXiphComment(tag, "DATE", metadata.GetReleaseDate());
	SetXiphComment(tag, "DESCRIPTION", metadata.GetComment());
	SetXiphComment(tag, "TITLE", metadata.GetTitle());
	SetXiphCommentNumber(tag, "TRACKNUMBER", metadata.GetTrackNumber());
	SetXiphCommentNumber(tag, "TRACKTOTAL", metadata.GetTrackTotal());
	SetXiphCommentBoolean(tag, "COMPILATION", metadata.GetCompilation());
	SetXiphCommentNumber(tag, "DISCNUMBER", metadata.GetDiscNumber());
	SetXiphCommentNumber(tag, "DISCTOTAL", metadata.GetDiscTotal());
	SetXiphComment(tag, "LYRICS", metadata.GetLyrics());
	SetXiphCommentNumber(tag, "BPM", metadata.GetBPM());
	SetXiphCommentNumber(tag, "RATING", metadata.GetRating());
	SetXiphComment(tag, "ISRC", metadata.GetISRC());
	SetXiphComment(tag, "MCN", metadata.GetMCN());
	SetXiphComment(tag, "MUSICBRAINZ_ALBUMID", metadata.GetMusicBrainzReleaseID());
	SetXiphComment(tag, "MUSICBRAINZ_TRACKID", metadata.GetMusicBrainzRecordingID());
	SetXiphComment(tag, "TITLESORT", metadata.GetTitleSortOrder());
	SetXiphComment(tag, "ALBUMTITLESORT", metadata.GetAlbumTitleSortOrder());
	SetXiphComment(tag, "ARTISTSORT", metadata.GetArtistSortOrder());
	SetXiphComment(tag, "ALBUMARTISTSORT", metadata.GetAlbumArtistSortOrder());
	SetXiphComment(tag, "COMPOSERSORT", metadata.GetComposerSortOrder());
	SetXiphComment(tag, "GROUPING", metadata.GetGrouping());

	// Additional metadata
	CFDictionaryRef additionalMetadata = metadata.GetAdditionalMetadata();
	if(nullptr != additionalMetadata) {
		CFIndex count = CFDictionaryGetCount(additionalMetadata);

		const void * keys [count];
		const void * values [count];

		CFDictionaryGetKeysAndValues(additionalMetadata, (const void **)keys, (const void **)values);

		for(CFIndex i = 0; i < count; ++i) {
			CFIndex keySize = CFStringGetMaximumSizeForEncoding(CFStringGetLength((CFStringRef)keys[i]), kCFStringEncodingASCII);
			char key [keySize + 1];

			if(!CFStringGetCString((CFStringRef)keys[i], key, keySize + 1, kCFStringEncodingASCII)) {
				LOGGER_ERR("org.sbooth.AudioEngine", "CFStringGetCString failed");
				continue;
			}

			SetXiphComment(tag, key, (CFStringRef)values[i]);
		}
	}

	// ReplayGain info
	SetXiphCommentDouble(tag, "REPLAYGAIN_REFERENCE_LOUDNESS", metadata.GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));
	SetXiphCommentDouble(tag, "REPLAYGAIN_TRACK_GAIN", metadata.GetReplayGainTrackGain(), CFSTR("%+2.2f dB"));
	SetXiphCommentDouble(tag, "REPLAYGAIN_TRACK_PEAK", metadata.GetReplayGainTrackPeak(), CFSTR("%1.8f"));
	SetXiphCommentDouble(tag, "REPLAYGAIN_ALBUM_GAIN", metadata.GetReplayGainAlbumGain(), CFSTR("%+2.2f dB"));
	SetXiphCommentDouble(tag, "REPLAYGAIN_ALBUM_PEAK", metadata.GetReplayGainAlbumPeak(), CFSTR("%1.8f"));

	// Album art
	if(setAlbumArt) {
		tag->removeFields("METADATA_BLOCK_PICTURE");

		for(auto attachedPicture : metadata.GetAttachedPictures()) {
			SFB::CGImageSource imageSource(CGImageSourceCreateWithData(attachedPicture->GetData(), nullptr));
			if(!imageSource)
				return false;

			TagLib::FLAC::Picture picture;
			picture.setData(TagLib::ByteVector((const char *)CFDataGetBytePtr(attachedPicture->GetData()), (size_t)CFDataGetLength(attachedPicture->GetData())));
			picture.setType((TagLib::FLAC::Picture::Type)attachedPicture->GetType());
			if(attachedPicture->GetDescription())
				picture.setDescription(TagLib::StringFromCFString(attachedPicture->GetDescription()));

			// Convert the image's UTI into a MIME type
			SFB::CFString mimeType(UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType));
			if(mimeType)
				picture.setMimeType(TagLib::StringFromCFString(mimeType));

			// Flesh out the height, width, and depth
			SFB::CFDictionary imagePropertiesDictionary(CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nullptr));
			if(imagePropertiesDictionary) {
				CFNumberRef imageWidth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelWidth);
				CFNumberRef imageHeight = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyPixelHeight);
				CFNumberRef imageDepth = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyDepth);

				int height, width, depth;

				// Ignore numeric conversion errors
				CFNumberGetValue(imageWidth, kCFNumberIntType, &width);
				CFNumberGetValue(imageHeight, kCFNumberIntType, &height);
				CFNumberGetValue(imageDepth, kCFNumberIntType, &depth);

				picture.setHeight(height);
				picture.setWidth(width);
				picture.setColorDepth(depth);
			}

			TagLib::ByteVector encodedBlock = TagLib::EncodeBase64(picture.render());
			tag->addField("METADATA_BLOCK_PICTURE", TagLib::String(encodedBlock, TagLib::String::UTF8), false);
		}
	}

	return true;
}
