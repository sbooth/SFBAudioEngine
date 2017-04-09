/*
 * Copyright (c) 2010 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/relativevolumeframe.h>
#include <taglib/popularimeterframe.h>
#include <taglib/textidentificationframe.h>

#include "AddID3v2TagToDictionary.h"
#include "AddTagToDictionary.h"
#include "AudioMetadata.h"
#include "CFWrapper.h"
#include "TagLibStringUtilities.h"
#include "CFDictionaryUtilities.h"

bool SFB::Audio::AddID3v2TagToDictionary(CFMutableDictionaryRef dictionary, std::vector<std::shared_ptr<AttachedPicture>>& attachedPictures, const TagLib::ID3v2::Tag *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	// Add the basic tags not specific to ID3v2
	AddTagToDictionary(dictionary, tag);

	// Release date
	auto frameList = tag->frameListMap()["TDRC"];
	if(!frameList.isEmpty()) {
		/*
		 The timestamp fields are based on a subset of ISO 8601. When being as
		 precise as possible the format of a time string is
		 yyyy-MM-ddTHH:mm:ss (year, "-", month, "-", day, "T", hour (out of
		 24), ":", minutes, ":", seconds), but the precision may be reduced by
		 removing as many time indicators as wanted. Hence valid timestamps
		 are
		 yyyy, yyyy-MM, yyyy-MM-dd, yyyy-MM-ddTHH, yyyy-MM-ddTHH:mm and
		 yyyy-MM-ddTHH:mm:ss. All time stamps are UTC. For durations, use
		 the slash character as described in 8601, and for multiple non-
		 contiguous dates, use multiple strings, if allowed by the frame
		 definition.
		 */

		TagLib::AddStringToCFDictionary(dictionary, Metadata::kReleaseDateKey, frameList.front()->toString());
	}

	// Extract composer if present
	frameList = tag->frameListMap()["TCOM"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kComposerKey, frameList.front()->toString());

	// Extract album artist
	frameList = tag->frameListMap()["TPE2"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kAlbumArtistKey, frameList.front()->toString());

	// BPM
	frameList = tag->frameListMap()["TBPM"];
	if(!frameList.isEmpty()) {
		bool ok = false;
		int BPM = frameList.front()->toString().toInt(&ok);
		if(ok)
			AddIntToDictionary(dictionary, Metadata::kBPMKey, BPM);
	}

	// Rating
	TagLib::ID3v2::PopularimeterFrame *popularimeter = nullptr;
	frameList = tag->frameListMap()["POPM"];
	if(!frameList.isEmpty() && nullptr != (popularimeter = dynamic_cast<TagLib::ID3v2::PopularimeterFrame *>(frameList.front())))
		AddIntToDictionary(dictionary, Metadata::kRatingKey, popularimeter->rating());

	// Extract total tracks if present
	frameList = tag->frameListMap()["TRCK"];
	if(!frameList.isEmpty()) {
		// Split the tracks at '/'
		TagLib::String s = frameList.front()->toString();

		bool ok;
		size_t pos = s.find("/", 0);
		if(TagLib::String::npos() != pos) {
			int trackNum = s.substr(0, pos).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, Metadata::kTrackNumberKey, trackNum);

			int trackTotal = s.substr(pos + 1).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, Metadata::kTrackTotalKey, trackTotal);
		}
		else if(s.length()) {
			int trackNum = s.toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, Metadata::kTrackNumberKey, trackNum);
		}
	}

	// Extract disc number and total discs
	frameList = tag->frameListMap()["TPOS"];
	if(!frameList.isEmpty()) {
		// Split the tracks at '/'
		TagLib::String s = frameList.front()->toString();

		bool ok;
		size_t pos = s.find("/", 0);
		if(TagLib::String::npos() != pos) {
			int discNum = s.substr(0, pos).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, Metadata::kDiscNumberKey, discNum);

			int discTotal = s.substr(pos + 1).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, Metadata::kDiscTotalKey, discTotal);
		}
		else if(s.length()) {
			int discNum = s.toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, Metadata::kDiscNumberKey, discNum);
		}
	}

	// Lyrics
	frameList = tag->frameListMap()["USLT"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kLyricsKey, frameList.front()->toString());

	// Extract compilation if present (iTunes TCMP tag)
	frameList = tag->frameListMap()["TCMP"];
	if(!frameList.isEmpty())
		// It seems that the presence of this frame indicates a compilation
		CFDictionarySetValue(dictionary, Metadata::kCompilationKey, kCFBooleanTrue);

	frameList = tag->frameListMap()["TSRC"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kISRCKey, frameList.front()->toString());

	// MusicBrainz
	auto musicBrainzReleaseIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Album Id");
	if(musicBrainzReleaseIDFrame)
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kMusicBrainzReleaseIDKey, musicBrainzReleaseIDFrame->fieldList().back());

	auto musicBrainzRecordingIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Track Id");
	if(musicBrainzRecordingIDFrame)
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kMusicBrainzRecordingIDKey, musicBrainzRecordingIDFrame->fieldList().back());

	// Sorting and grouping
	frameList = tag->frameListMap()["TSOT"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kTitleSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSOA"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kAlbumTitleSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSOP"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kArtistSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSO2"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kAlbumArtistSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSOC"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kComposerSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TIT1"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, Metadata::kGroupingKey, frameList.front()->toString());

	// ReplayGain
	bool foundReplayGain = false;

	// Preference is TXXX frames, RVA2 frame, then LAME header
	auto trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_TRACK_GAIN");
	auto trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_TRACK_PEAK");
	auto albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_ALBUM_GAIN");
	auto albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_ALBUM_PEAK");

	if(!trackGainFrame)
		trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_track_gain");
	if(trackGainFrame) {
		SFB::CFString str(trackGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);

		AddDoubleToDictionary(dictionary, Metadata::kTrackGainKey, num);
		AddDoubleToDictionary(dictionary, Metadata::kReferenceLoudnessKey, 89.0);

		foundReplayGain = true;
	}

	if(!trackPeakFrame)
		trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_track_peak");
	if(trackPeakFrame) {
		SFB::CFString str(trackPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);

		AddDoubleToDictionary(dictionary, Metadata::kTrackPeakKey, num);
	}

	if(!albumGainFrame)
		albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_gain");
	if(albumGainFrame) {
		SFB::CFString str(albumGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);

		AddDoubleToDictionary(dictionary, Metadata::kAlbumGainKey, num);
		AddDoubleToDictionary(dictionary, Metadata::kReferenceLoudnessKey, 89.0);

		foundReplayGain = true;
	}

	if(!albumPeakFrame)
		albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_peak");
	if(albumPeakFrame) {
		SFB::CFString str(albumPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);

		AddDoubleToDictionary(dictionary, Metadata::kAlbumPeakKey, num);
	}

	// If nothing found check for RVA2 frame
	if(!foundReplayGain) {
		frameList = tag->frameListMap()["RVA2"];

		for(auto frameIterator : tag->frameListMap()["RVA2"]) {
			TagLib::ID3v2::RelativeVolumeFrame *relativeVolume = dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame *>(frameIterator);
			if(!relativeVolume)
				continue;

			// Attempt to use the master volume if present
			auto channels		= relativeVolume->channels();
			auto channelType	= TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;

			// Fall back on whatever else exists in the frame
			if(!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume))
				channelType = channels.front();

			float volumeAdjustment = relativeVolume->volumeAdjustment(channelType);

			if(TagLib::String("track", TagLib::String::Latin1) == relativeVolume->identification()) {
				if((int)volumeAdjustment)
					AddFloatToDictionary(dictionary, Metadata::kTrackGainKey, volumeAdjustment);
			}
			else if(TagLib::String("album", TagLib::String::Latin1) == relativeVolume->identification()) {
				if((int)volumeAdjustment)
					AddFloatToDictionary(dictionary, Metadata::kAlbumGainKey, volumeAdjustment);
			}
			// Fall back to track gain if identification is not specified
			else {
				if((int)volumeAdjustment)
					AddFloatToDictionary(dictionary, Metadata::kTrackGainKey, volumeAdjustment);
			}
		}
	}

	// Extract album art if present
	for(auto it : tag->frameListMap()["APIC"]) {
		TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(it);
		if(frame) {
			SFB::CFData data((const UInt8 *)frame->picture().data(), (CFIndex)frame->picture().size());

			SFB::CFString description;
			if(!frame->description().isEmpty())
				description = CFString(frame->description().toCString(true), kCFStringEncodingUTF8);

			attachedPictures.push_back(std::make_shared<AttachedPicture>(data, (AttachedPicture::Type)frame->type(), description));
		}
	}

	return true;
}
