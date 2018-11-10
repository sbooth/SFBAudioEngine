/*
 * Copyright (c) 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/mp4coverart.h>
#include <ApplicationServices/ApplicationServices.h>

#include "SetMP4TagFromMetadata.h"
#include "AudioMetadata.h"
#include "CFWrapper.h"
#include "TagLibStringUtilities.h"
#include "Logger.h"

// ========================================
// MP4 item utilities
namespace {

	bool SetMP4Item(TagLib::MP4::Tag *tag, const char *key, CFStringRef value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing item with this name
		tag->removeItem(key);

		// Nothing left to do if value is nullptr
		if(nullptr == value)
			return true;

		tag->setItem(key, TagLib::MP4::Item(TagLib::StringFromCFString(value)));

		return true;
	}

	bool SetMP4ItemInt(TagLib::MP4::Tag *tag, const char *key, CFNumberRef value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing item with this name
		tag->removeItem(key);

		// Nothing left to do if value is nullptr
		if(nullptr == value)
			return true;

		int i;
		if(!CFNumberGetValue(value, kCFNumberIntType, &i))
			return false;

		tag->setItem(key, TagLib::MP4::Item(i));

		return true;
	}

	bool SetMP4ItemIntPair(TagLib::MP4::Tag *tag, const char *key, CFNumberRef valueOne, CFNumberRef valueTwo)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing item with this name
		tag->removeItem(key);

		// Nothing left to do if value is nullptr
		if(nullptr == valueOne && nullptr == valueTwo)
			return true;

		int i = 0, j = 0;
		if(valueOne != nullptr && !CFNumberGetValue(valueOne, kCFNumberIntType, &i))
			return false;
		if(valueTwo != nullptr && !CFNumberGetValue(valueTwo, kCFNumberIntType, &j))
			return false;

		tag->setItem(key, TagLib::MP4::Item(i, j));

		return true;
	}

	bool SetMP4ItemBoolean(TagLib::MP4::Tag *tag, const char *key, CFBooleanRef value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		if(nullptr == value)
			return SetMP4Item(tag, key, nullptr);
		else if(CFBooleanGetValue(value))
			tag->setItem(key, TagLib::MP4::Item(1));
		else
			tag->setItem(key, TagLib::MP4::Item(0));

		return true;
	}

	bool SetMP4ItemDouble(TagLib::MP4::Tag *tag, const char *key, CFNumberRef value, CFStringRef format = nullptr)
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

		bool result = SetMP4Item(tag, key, numberString);

		return result;
	}

}

bool SFB::Audio::SetMP4TagFromMetadata(const Metadata& metadata, TagLib::MP4::Tag *tag, bool setAlbumArt)
{
	if(nullptr == tag)
		return false;

	SetMP4Item(tag, "\251nam", metadata.GetTitle());
	SetMP4Item(tag, "\251ART", metadata.GetArtist());
	SetMP4Item(tag, "\251ALB", metadata.GetAlbumTitle());
	SetMP4Item(tag, "aART", metadata.GetAlbumArtist());
	SetMP4Item(tag, "\251gen", metadata.GetGenre());
	SetMP4Item(tag, "\251wrt", metadata.GetComposer());
	SetMP4Item(tag, "\251cmt", metadata.GetComment());
	SetMP4Item(tag, "\251day", metadata.GetReleaseDate());

	SetMP4ItemIntPair(tag, "trkn", metadata.GetTrackNumber(), metadata.GetTrackTotal());
	SetMP4ItemIntPair(tag, "disk", metadata.GetDiscNumber(), metadata.GetDiscTotal());

	SetMP4ItemBoolean(tag, "cpil", metadata.GetCompilation());

	SetMP4ItemInt(tag, "tmpo", metadata.GetBPM());

	SetMP4Item(tag, "\251lyr", metadata.GetLyrics());

	// Sorting
	SetMP4Item(tag, "sonm", metadata.GetTitleSortOrder());
	SetMP4Item(tag, "soal", metadata.GetAlbumTitleSortOrder());
	SetMP4Item(tag, "soar", metadata.GetArtistSortOrder());
	SetMP4Item(tag, "soaa", metadata.GetAlbumArtistSortOrder());
	SetMP4Item(tag, "soco", metadata.GetComposerSortOrder());

	SetMP4Item(tag, "\251grp", metadata.GetGrouping());

	// MusicBrainz
	SetMP4Item(tag, "---:com.apple.iTunes:MusicBrainz Album Id", metadata.GetMusicBrainzReleaseID());
	SetMP4Item(tag, "---:com.apple.iTunes:MusicBrainz Track Id", metadata.GetMusicBrainzRecordingID());

	// ReplayGain info
	SetMP4ItemDouble(tag, "---:com.apple.iTunes:replaygain_reference_loudness", metadata.GetReplayGainReferenceLoudness(), CFSTR("%2.1f dB"));
	SetMP4ItemDouble(tag, "---:com.apple.iTunes:replaygain_track_gain", metadata.GetReplayGainTrackGain(), CFSTR("%2.2f dB"));
	SetMP4ItemDouble(tag, "---:com.apple.iTunes:replaygain_track_peak", metadata.GetReplayGainTrackPeak(), CFSTR("%1.8f dB"));
	SetMP4ItemDouble(tag, "---:com.apple.iTunes:replaygain_album_gain", metadata.GetReplayGainAlbumGain(), CFSTR("%2.2f dB"));
	SetMP4ItemDouble(tag, "---:com.apple.iTunes:replaygain_album_peak", metadata.GetReplayGainAlbumPeak(), CFSTR("%1.8f dB"));

	if(setAlbumArt) {
		auto list = TagLib::MP4::CoverArtList();
		for(auto attachedPicture : metadata.GetAttachedPictures()) {
			SFB::CGImageSource imageSource(CGImageSourceCreateWithData(attachedPicture->GetData(), nullptr));
			if(!imageSource)
				continue;

			// Convert the image's UTI into a MIME type
			SFB::CFString mimeType(UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType));
			auto type = TagLib::MP4::CoverArt::CoverArt::Unknown;
			if(mimeType) {
				if(UTTypeEqual(kUTTypeBMP, mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::BMP;
				else if(UTTypeEqual(kUTTypePNG, mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::PNG;
				else if(UTTypeEqual(kUTTypeGIF, mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::GIF;
				else if(UTTypeEqual(kUTTypeJPEG, mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::JPEG;
			}

			auto picture = TagLib::MP4::CoverArt(type, TagLib::ByteVector((const char *)CFDataGetBytePtr(attachedPicture->GetData()), (size_t)CFDataGetLength(attachedPicture->GetData())));
			list.append(picture);
		}

		tag->setItem("covr", list);
	}

	return true;
}
