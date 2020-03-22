/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/flacpicture.h>

#include "Base64Utilities.h"
#include "CFWrapper.h"
#include "SetXiphCommentFromMetadata.h"
#include "TagLibStringUtilities.h"

// ========================================
// Xiph comment utilities
namespace {

	void SetXiphComment(TagLib::Ogg::XiphComment *tag, const char *key, NSString *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing comment with this name
		tag->removeFields(key);

		if(value)
			tag->addField(key, TagLib::StringFromNSString(value));
	}

	void SetXiphCommentNumber(TagLib::Ogg::XiphComment *tag, const char *key, NSNumber *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		SetXiphComment(tag, key, value.stringValue);
	}

	void SetXiphCommentBoolean(TagLib::Ogg::XiphComment *tag, const char *key, NSNumber *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		if(!value)
			SetXiphComment(tag, key, nil);
		else
			SetXiphComment(tag, key, value.boolValue ? @"1" : @"0");
	}

	void SetXiphCommentDoubleWithFormat(TagLib::Ogg::XiphComment *tag, const char *key, NSNumber *value, NSString *format = nil)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		SetXiphComment(tag, key, value ? [NSString stringWithFormat:(format ?: @"%f"), value.doubleValue] : nil);
	}

}

void SFB::Audio::SetXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt)
{
	NSCParameterAssert(metadata != nil);
	assert(nullptr != tag);

	// Standard tags
	SetXiphComment(tag, "ALBUM", metadata.albumTitle);
	SetXiphComment(tag, "ARTIST", metadata.artist);
	SetXiphComment(tag, "ALBUMARTIST", metadata.albumArtist);
	SetXiphComment(tag, "COMPOSER", metadata.composer);
	SetXiphComment(tag, "GENRE", metadata.genre);
	SetXiphComment(tag, "DATE", metadata.releaseDate);
	SetXiphComment(tag, "DESCRIPTION", metadata.comment);
	SetXiphComment(tag, "TITLE", metadata.title);
	SetXiphCommentNumber(tag, "TRACKNUMBER", metadata.trackNumber);
	SetXiphCommentNumber(tag, "TRACKTOTAL", metadata.trackTotal);
	SetXiphCommentBoolean(tag, "COMPILATION", metadata.compilation);
	SetXiphCommentNumber(tag, "DISCNUMBER", metadata.discNumber);
	SetXiphCommentNumber(tag, "DISCTOTAL", metadata.discTotal);
	SetXiphComment(tag, "LYRICS", metadata.lyrics);
	SetXiphCommentNumber(tag, "BPM", metadata.bpm);
	SetXiphCommentNumber(tag, "RATING", metadata.rating);
	SetXiphComment(tag, "ISRC", metadata.isrc);
	SetXiphComment(tag, "MCN", metadata.mcn);
	SetXiphComment(tag, "MUSICBRAINZ_ALBUMID", metadata.musicBrainzReleaseID);
	SetXiphComment(tag, "MUSICBRAINZ_TRACKID", metadata.musicBrainzRecordingID);
	SetXiphComment(tag, "TITLESORT", metadata.titleSortOrder);
	SetXiphComment(tag, "ALBUMTITLESORT", metadata.albumTitleSortOrder);
	SetXiphComment(tag, "ARTISTSORT", metadata.artistSortOrder);
	SetXiphComment(tag, "ALBUMARTISTSORT", metadata.albumArtistSortOrder);
	SetXiphComment(tag, "COMPOSERSORT", metadata.composerSortOrder);
	SetXiphComment(tag, "GROUPING", metadata.grouping);

	// Additional metadata
	NSDictionary *additionalMetadata = metadata.additionalMetadata;
	if(additionalMetadata) {
		for(NSString *key in additionalMetadata)
			SetXiphComment(tag, key.UTF8String, additionalMetadata[key]);
	}

	// ReplayGain info
	SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_REFERENCE_LOUDNESS", metadata.replayGainReferenceLoudness, @"%2.1f dB");
	SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_TRACK_GAIN", metadata.replayGainTrackGain, @"%+2.2f dB");
	SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_TRACK_PEAK", metadata.replayGainTrackPeak, @"%1.8f");
	SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_ALBUM_GAIN", metadata.replayGainAlbumGain, @"%+2.2f dB");
	SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_ALBUM_PEAK", metadata.replayGainAlbumPeak, @"%1.8f");

	// Album art
	if(setAlbumArt) {
		tag->removeFields("METADATA_BLOCK_PICTURE");

		for(SFBAttachedPicture *attachedPicture in metadata.attachedPictures) {
			SFB::CGImageSource imageSource(CGImageSourceCreateWithData((__bridge CFDataRef)attachedPicture.imageData, nullptr));
			if(!imageSource)
				continue;

			TagLib::FLAC::Picture picture;
			picture.setData(TagLib::ByteVector((const char *)attachedPicture.imageData.bytes, (size_t)attachedPicture.imageData.length));
			picture.setType((TagLib::FLAC::Picture::Type)attachedPicture.pictureType);
			if(attachedPicture.pictureDescription)
				picture.setDescription(TagLib::StringFromNSString(attachedPicture.pictureDescription));

			// Convert the image's UTI into a MIME type
			NSString *mimeType = (__bridge_transfer NSString *)UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
			if(mimeType)
				picture.setMimeType(TagLib::StringFromNSString(mimeType));

			// Flesh out the height, width, and depth
			NSDictionary *imagePropertiesDictionary = (__bridge_transfer NSDictionary *)CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nullptr);
			if(imagePropertiesDictionary) {
				NSNumber *imageWidth = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyPixelWidth];
				NSNumber *imageHeight = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyPixelHeight];
				NSNumber *imageDepth = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyDepth];

				picture.setHeight(imageHeight.intValue);
				picture.setWidth(imageWidth.intValue);
				picture.setColorDepth(imageDepth.intValue);
			}

			TagLib::ByteVector encodedBlock = TagLib::EncodeBase64(picture.render());
			tag->addField("METADATA_BLOCK_PICTURE", TagLib::String(encodedBlock, TagLib::String::UTF8), false);
		}
	}
}
