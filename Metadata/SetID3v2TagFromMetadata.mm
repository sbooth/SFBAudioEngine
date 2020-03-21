/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/attachedpictureframe.h>
#include <taglib/id3v2frame.h>
#include <taglib/popularimeterframe.h>
#include <taglib/relativevolumeframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/unsynchronizedlyricsframe.h>

#include "CFWrapper.h"
#include "SetID3v2TagFromMetadata.h"
#include "TagLibStringUtilities.h"

void SFB::Audio::SetID3v2TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt)
{
	NSCParameterAssert(metadata != nil);
	assert(nullptr != tag);

	// Use UTF-8 as the default encoding
	(TagLib::ID3v2::FrameFactory::instance())->setDefaultTextEncoding(TagLib::String::UTF8);

	// Album title
	tag->setAlbum(TagLib::StringFromNSString(metadata.albumTitle));

	// Artist
	tag->setArtist(TagLib::StringFromNSString(metadata.artist));

	// Composer
	tag->removeFrames("TCOM");
	if(metadata.composer) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString(metadata.composer));
		tag->addFrame(frame);
	}

	// Genre
	tag->setGenre(TagLib::StringFromNSString(metadata.genre));

	// Date
	tag->removeFrames("TDRC");
	if(metadata.releaseDate) {
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
		NSISO8601DateFormatter *formatter = [[NSISO8601DateFormatter alloc] init];
		NSCalendar *gregorianCalendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
		tag->setYear((unsigned int)[gregorianCalendar component:NSCalendarUnitYear fromDate:[formatter dateFromString:metadata.releaseDate]]);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TDRC", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString(metadata.releaseDate));
		tag->addFrame(frame);
	}

	// Comment
	tag->setComment(TagLib::StringFromNSString(metadata.comment));

	// Album artist
	tag->removeFrames("TPE2");
	if(metadata.albumArtist) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPE2", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString(metadata.albumArtist));
		tag->addFrame(frame);
	}

	// Track title
	tag->setTitle(TagLib::StringFromNSString(metadata.title));

	// BPM
	tag->removeFrames("TBPM");
	if(metadata.bpm) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TBPM", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString(metadata.bpm.stringValue));
		tag->addFrame(frame);
	}

	// Rating
	tag->removeFrames("POPM");
	if(metadata.rating) {
		TagLib::ID3v2::PopularimeterFrame *frame = new TagLib::ID3v2::PopularimeterFrame();
		frame->setRating(metadata.rating.intValue);
		tag->addFrame(frame);
	}

	// Track number and total tracks
	tag->removeFrames("TRCK");
	if(metadata.trackNumber && metadata.trackTotal) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@/%@", metadata.trackNumber, metadata.trackTotal]));
		tag->addFrame(frame);
	}
	else if(metadata.trackNumber) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@", metadata.trackNumber]));
		tag->addFrame(frame);
	}
	else if(metadata.trackTotal) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"/%@", metadata.trackTotal]));
		tag->addFrame(frame);
	}

	// Compilation
	// iTunes uses the TCMP frame for this, which isn't in the standard, but we'll use it for compatibility
	tag->removeFrames("TCMP");
	if(metadata.compilation) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TCMP", TagLib::String::Latin1);
		frame->setText(metadata.compilation.boolValue ? "1" : "0");
		tag->addFrame(frame);
	}

	// Disc number and total discs
	tag->removeFrames("TPOS");
	if(metadata.discNumber && metadata.discTotal) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@/%@", metadata.discNumber, metadata.discTotal]));
		tag->addFrame(frame);
	}
	else if(metadata.discNumber) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@", metadata.discNumber]));
		tag->addFrame(frame);
	}
	else if(metadata.discTotal) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"/%@", metadata.discTotal]));
		tag->addFrame(frame);
	}

	// Lyrics
	tag->removeFrames("USLT");
	if(metadata.lyrics) {
		auto frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(TagLib::String::UTF8);
		frame->setText(TagLib::StringFromNSString(metadata.lyrics));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSRC");
	if(metadata.isrc) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSRC", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromNSString(metadata.isrc));
		tag->addFrame(frame);
	}

	// MusicBrainz
	auto musicBrainzReleaseIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "MusicBrainz Album Id");
	if(nullptr != musicBrainzReleaseIDFrame)
		tag->removeFrame(musicBrainzReleaseIDFrame);

	if(metadata.musicBrainzReleaseID) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("MusicBrainz Album Id");
		frame->setText(TagLib::StringFromNSString(metadata.musicBrainzReleaseID));
		tag->addFrame(frame);
	}


	auto musicBrainzRecordingIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Track Id");
	if(nullptr != musicBrainzRecordingIDFrame)
		tag->removeFrame(musicBrainzRecordingIDFrame);

	if(metadata.musicBrainzRecordingID) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("MusicBrainz Track Id");
		frame->setText(TagLib::StringFromNSString(metadata.musicBrainzRecordingID));
		tag->addFrame(frame);
	}

	// Sorting and grouping
	tag->removeFrames("TSOT");
	if(metadata.titleSortOrder) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOT", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromNSString(metadata.titleSortOrder));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSOA");
	if(metadata.albumTitleSortOrder) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOA", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromNSString(metadata.albumTitleSortOrder));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSOP");
	if(metadata.artistSortOrder) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOP", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromNSString(metadata.artistSortOrder));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSO2");
	if(metadata.albumArtistSortOrder) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSO2", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromNSString(metadata.albumArtistSortOrder));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSOC");
	if(metadata.composerSortOrder) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOC", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromNSString(metadata.composerSortOrder));
		tag->addFrame(frame);
	}

	tag->removeFrames("TIT1");
	if(metadata.grouping) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TIT1", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromNSString(metadata.grouping));
		tag->addFrame(frame);
	}

	// ReplayGain

	// Write TXXX frames
	auto trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_track_gain");
	auto trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_track_peak");
	auto albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_album_gain");
	auto albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_album_peak");

	if(nullptr != trackGainFrame)
		tag->removeFrame(trackGainFrame);

	if(nullptr != trackPeakFrame)
		tag->removeFrame(trackPeakFrame);

	if(nullptr != albumGainFrame)
		tag->removeFrame(albumGainFrame);

	if(nullptr != albumPeakFrame)
		tag->removeFrame(albumPeakFrame);

	if(metadata.replayGainTrackGain) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_track_gain");
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%+2.2f dB", metadata.replayGainTrackGain.doubleValue]));
		tag->addFrame(frame);
	}

	if(metadata.replayGainTrackPeak) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_track_peak");
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%1.8f dB", metadata.replayGainTrackPeak.doubleValue]));
		tag->addFrame(frame);
	}

	if(metadata.replayGainAlbumGain) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_album_gain");
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%+2.2f dB", metadata.replayGainAlbumGain.doubleValue]));
		tag->addFrame(frame);
	}

	if(metadata.replayGainAlbumPeak) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_album_peak");
		frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%1.8f dB", metadata.replayGainAlbumPeak.doubleValue]));
		tag->addFrame(frame);
	}

	// Also write the RVA2 frames
	tag->removeFrames("RVA2");
	if(metadata.replayGainTrackGain) {
		auto relativeVolume = new TagLib::ID3v2::RelativeVolumeFrame();
		relativeVolume->setIdentification(TagLib::String("track", TagLib::String::Latin1));
		relativeVolume->setVolumeAdjustment(metadata.replayGainTrackGain.floatValue, TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);
		tag->addFrame(relativeVolume);
	}

	if(metadata.replayGainAlbumGain) {
		auto relativeVolume = new TagLib::ID3v2::RelativeVolumeFrame();
		relativeVolume->setIdentification(TagLib::String("album", TagLib::String::Latin1));
		relativeVolume->setVolumeAdjustment(metadata.replayGainAlbumGain.floatValue, TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);
		tag->addFrame(relativeVolume);
	}

	// Album art
	if(setAlbumArt) {
		tag->removeFrames("APIC");

		for(SFBAttachedPicture *attachedPicture in metadata.attachedPictures) {
			SFB::CGImageSource imageSource(CGImageSourceCreateWithData((__bridge CFDataRef)attachedPicture.imageData, nullptr));
			if(!imageSource)
				continue;

			TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;

			// Convert the image's UTI into a MIME type
			NSString *mimeType = (__bridge_transfer NSString *)UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
			if(mimeType)
				frame->setMimeType(TagLib::StringFromNSString(mimeType));

			frame->setPicture(TagLib::ByteVector((const char *)attachedPicture.imageData.bytes, (size_t)attachedPicture.imageData.length));
			frame->setType((TagLib::ID3v2::AttachedPictureFrame::Type)attachedPicture.pictureType);
			if(attachedPicture.pictureDescription)
				frame->setDescription(TagLib::StringFromNSString(attachedPicture.pictureDescription));
			tag->addFrame(frame);
		}
	}
}
