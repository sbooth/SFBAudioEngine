/*
 * Copyright (c) 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/mp4coverart.h>

#include "AddMP4TagToDictionary.h"
#include "AddTagToDictionary.h"
#include "AudioMetadata.h"
#include "TagLibStringUtilities.h"
#include "CFDictionaryUtilities.h"

bool SFB::Audio::AddMP4TagToDictionary(CFMutableDictionaryRef dictionary, std::vector<std::shared_ptr<AttachedPicture>>& attachedPictures, const TagLib::MP4::Tag *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	// Add the basic tags not specific to MP4
	AddTagToDictionary(dictionary, tag);

	if(tag->contains("aART")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kAlbumArtistKey, tag->item("aART").toString());
	}
	if(tag->contains("\251wrt")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kComposerKey, tag->item("\251wrt").toString());
	}
	if(tag->contains("\251day")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kReleaseDateKey, tag->item("\251day").toString());
	}

	if(tag->contains("trkn")) {
		auto track = tag->item("trkn").toIntPair();
		if(track.first)
			AddIntToDictionary(dictionary, Metadata::kTrackNumberKey, track.first);
		if(track.second)
			AddIntToDictionary(dictionary, Metadata::kTrackTotalKey, track.second);
	}
	if(tag->contains("disk")) {
		auto disc = tag->item("disk").toIntPair();
		if(disc.first)
			AddIntToDictionary(dictionary, Metadata::kDiscNumberKey, disc.first);
		if(disc.second)
			AddIntToDictionary(dictionary, Metadata::kDiscTotalKey, disc.second);
	}
	if(tag->contains("cpil")) {
		if(tag->item("cpil").toBool())
			CFDictionarySetValue(dictionary, Metadata::kCompilationKey, kCFBooleanTrue);
	}
	if(tag->contains("tmpo")) {
		auto bpm = tag->item("tmpo").toInt();
		if(bpm)
			AddIntToDictionary(dictionary, Metadata::kBPMKey, bpm);
	}
	if(tag->contains("\251lyr")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kLyricsKey, tag->item("\251lyr").toString());
	}

	// Sorting
	if(tag->contains("sonm")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kTitleSortOrderKey, tag->item("sonm").toString());
	}
	if(tag->contains("soal")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kAlbumTitleSortOrderKey, tag->item("soal").toString());
	}
	if(tag->contains("soar")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kArtistSortOrderKey, tag->item("soar").toString());
	}
	if(tag->contains("soaa")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kAlbumArtistSortOrderKey, tag->item("soaa").toString());
	}
	if(tag->contains("soco")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kComposerSortOrderKey, tag->item("soco").toString());
	}

	if(tag->contains("\251grp")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kGroupingKey, tag->item("\251grp").toString());
	}

	// Album art
	if(tag->contains("covr")) {
		auto art = tag->item("covr").toCoverArtList();
		for(auto iter : art) {
			SFB::CFData data((const UInt8 *)iter.data().data(), (CFIndex)iter.data().size());

			SFB::CFString description;
//			if(!frame->description().isEmpty())
//				description = CFString(frame->description().toCString(true), kCFStringEncodingUTF8);

			attachedPictures.push_back(std::make_shared<AttachedPicture>(data, AttachedPicture::Type::Other, description));
		}
	}

	// MusicBrainz
	if(tag->contains("---:com.apple.iTunes:MusicBrainz Album Id")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kMusicBrainzReleaseIDKey, tag->item("---:com.apple.iTunes:MusicBrainz Album Id").toString());
	}

	if(tag->contains("---:com.apple.iTunes:MusicBrainz Track Id")) {
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kMusicBrainzRecordingIDKey, tag->item("---:com.apple.iTunes:MusicBrainz Track Id").toString());
	}

	// ReplayGain
	if(tag->contains("---:com.apple.iTunes:replaygain_reference_loudness")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_reference_loudness").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			AddFloatToDictionary(dictionary, Metadata::kReferenceLoudnessKey, f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_track_gain")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_track_gain").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			AddFloatToDictionary(dictionary, Metadata::kTrackGainKey, f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_track_peak")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_track_peak").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			AddFloatToDictionary(dictionary, Metadata::kTrackPeakKey, f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_album_gain")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_album_gain").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			AddFloatToDictionary(dictionary, Metadata::kAlbumGainKey, f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_album_peak")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_album_peak").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			AddFloatToDictionary(dictionary, Metadata::kAlbumPeakKey, f);
	}

	return true;
}
