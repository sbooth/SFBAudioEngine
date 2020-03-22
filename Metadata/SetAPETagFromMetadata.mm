/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "SetAPETagFromMetadata.h"
#include "TagLibStringUtilities.h"

// ========================================
// APE tag utilities
namespace {

	void SetAPETag(TagLib::APE::Tag *tag, const char *key, NSString *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing comment with this name
		tag->removeItem(key);

		if(value)
			tag->addValue(key, TagLib::StringFromNSString(value));
	}

	void SetAPETagNumber(TagLib::APE::Tag *tag, const char *key, NSNumber *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		SetAPETag(tag, key, value.stringValue);
	}

	void SetAPETagBoolean(TagLib::APE::Tag *tag, const char *key, NSNumber *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		if(!value)
			SetAPETag(tag, key, nil);
		else
			SetAPETag(tag, key, value.boolValue ? @"1" : @"0");
	}

	void SetAPETagDoubleWithFormat(TagLib::APE::Tag *tag, const char *key, NSNumber *value, NSString *format = nil)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		SetAPETag(tag, key, value ? [NSString stringWithFormat:(format ?: @"%f"), value.doubleValue] : nil);
	}

}

void SFB::Audio::SetAPETagFromMetadata(SFBAudioMetadata *metadata, TagLib::APE::Tag *tag, bool setAlbumArt)
{
	NSCParameterAssert(metadata != nil);
	assert(nullptr != tag);

	// Standard tags
	SetAPETag(tag, "ALBUM", metadata.albumTitle);
	SetAPETag(tag, "ARTIST", metadata.artist);
	SetAPETag(tag, "ALBUMARTIST", metadata.albumArtist);
	SetAPETag(tag, "COMPOSER", metadata.composer);
	SetAPETag(tag, "GENRE", metadata.genre);
	SetAPETag(tag, "DATE", metadata.releaseDate);
	SetAPETag(tag, "DESCRIPTION", metadata.comment);
	SetAPETag(tag, "TITLE", metadata.title);
	SetAPETagNumber(tag, "TRACKNUMBER", metadata.trackNumber);
	SetAPETagNumber(tag, "TRACKTOTAL", metadata.trackTotal);
	SetAPETagBoolean(tag, "COMPILATION", metadata.compilation);
	SetAPETagNumber(tag, "DISCNUMBER", metadata.discNumber);
	SetAPETagNumber(tag, "DISCTOTAL", metadata.discTotal);
	SetAPETagNumber(tag, "BPM", metadata.bpm);
	SetAPETagNumber(tag, "RATING", metadata.rating);
	SetAPETag(tag, "ISRC", metadata.isrc);
	SetAPETag(tag, "MCN", metadata.mcn);
	SetAPETag(tag, "MUSICBRAINZ_ALBUMID", metadata.musicBrainzReleaseID);
	SetAPETag(tag, "MUSICBRAINZ_TRACKID", metadata.musicBrainzRecordingID);
	SetAPETag(tag, "TITLESORT", metadata.titleSortOrder);
	SetAPETag(tag, "ALBUMTITLESORT", metadata.albumTitleSortOrder);
	SetAPETag(tag, "ARTISTSORT", metadata.artistSortOrder);
	SetAPETag(tag, "ALBUMARTISTSORT", metadata.albumArtistSortOrder);
	SetAPETag(tag, "COMPOSERSORT", metadata.composerSortOrder);
	SetAPETag(tag, "GROUPING", metadata.grouping);

	// Additional metadata
	NSDictionary *additionalMetadata = metadata.additionalMetadata;
	if(additionalMetadata) {
		for(NSString *key in additionalMetadata)
			SetAPETag(tag, key.UTF8String, additionalMetadata[key]);
	}

	// ReplayGain info
	SetAPETagDoubleWithFormat(tag, "REPLAYGAIN_REFERENCE_LOUDNESS", metadata.replayGainReferenceLoudness, @"%2.1f dB");
	SetAPETagDoubleWithFormat(tag, "REPLAYGAIN_TRACK_GAIN", metadata.replayGainTrackGain, @"%+2.2f dB");
	SetAPETagDoubleWithFormat(tag, "REPLAYGAIN_TRACK_PEAK", metadata.replayGainTrackPeak, @"%1.8f");
	SetAPETagDoubleWithFormat(tag, "REPLAYGAIN_ALBUM_GAIN", metadata.replayGainAlbumGain, @"%+2.2f dB");
	SetAPETagDoubleWithFormat(tag, "REPLAYGAIN_ALBUM_PEAK", metadata.replayGainAlbumPeak, @"%1.8f");

	// Album art
	if(setAlbumArt) {
		tag->removeItem("Cover Art (Front)");
		tag->removeItem("Cover Art (Back)");

		for(SFBAttachedPicture *attachedPicture in metadata.attachedPictures) {
			// APE can handle front and back covers natively
			if(SFBAttachedPictureTypeFrontCover == attachedPicture.pictureType || SFBAttachedPictureTypeBackCover == attachedPicture.pictureType) {
				TagLib::ByteVector data;

				if(attachedPicture.pictureDescription)
					data.append(TagLib::StringFromNSString(attachedPicture.pictureDescription).data(TagLib::String::UTF8));
				data.append('\0');
				data.append(TagLib::ByteVector((const char *)attachedPicture.imageData.bytes, (size_t)attachedPicture.imageData.length));

				if(SFBAttachedPictureTypeFrontCover == attachedPicture.pictureType)
					tag->setData("Cover Art (Front)", data);
				else if(SFBAttachedPictureTypeBackCover == attachedPicture.pictureType)
					tag->setData("Cover Art (Back)", data);
			}
		}
	}
}
