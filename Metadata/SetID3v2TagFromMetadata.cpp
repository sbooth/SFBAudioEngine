/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/relativevolumeframe.h>
#include <taglib/popularimeterframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <ApplicationServices/ApplicationServices.h>

#include "SetID3v2TagFromMetadata.h"
#include "AudioMetadata.h"
#include "CFWrapper.h"
#include "TagLibStringUtilities.h"

namespace {

	// If type and format don't match, watch out!
	CFStringRef CreateStringFromNumberWithFormat(CFNumberRef	value,
												 CFNumberType	type,
												 CFStringRef	format = nullptr)
	{
		assert(nullptr != value);

		switch(type) {
				// Double
			case kCFNumberDoubleType:
			{
				double d;
				CFNumberGetValue(value, type, &d);

				return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, format ?: CFSTR("%f"), d);
			}

				// Everything else
			default:
				return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@"), value);
		}

		return nullptr;
	}

}

bool SFB::Audio::SetID3v2TagFromMetadata(const Metadata& metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt)
{
	if(nullptr == tag)
		return false;

	// Use UTF-8 as the default encoding
	(TagLib::ID3v2::FrameFactory::instance())->setDefaultTextEncoding(TagLib::String::UTF8);

	// Album title
	tag->setAlbum(TagLib::StringFromCFString(metadata.GetAlbumTitle()));

	// Artist
	tag->setArtist(TagLib::StringFromCFString(metadata.GetArtist()));

	// Composer
	tag->removeFrames("TCOM");
	if(metadata.GetComposer()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(metadata.GetComposer()));
		tag->addFrame(frame);
	}

	// Genre
	tag->setGenre(TagLib::StringFromCFString(metadata.GetGenre()));

	// Date
#if 1
	int year = 0;
	if(metadata.GetReleaseDate())
		year = CFStringGetIntValue(metadata.GetReleaseDate());
	tag->setYear((unsigned int)year);
#else
	// TODO: Parse the release date into components and set the frame appropriately
	tag->removeFrames("TDRC");
	if(metadata.GetReleaseDate()) {
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
//		year = CFStringGetIntValue(metadata.GetReleaseDate());
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TDRC", TagLib::String::Latin1);
		frame->setText("");
		tag->addFrame(frame);
	}
#endif

	// Comment
	tag->setComment(TagLib::StringFromCFString(metadata.GetComment()));

	// Album artist
	tag->removeFrames("TPE2");
	if(metadata.GetAlbumArtist()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPE2", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(metadata.GetAlbumArtist()));
		tag->addFrame(frame);
	}

	// Track title
	tag->setTitle(TagLib::StringFromCFString(metadata.GetTitle()));

	// BPM
	tag->removeFrames("TBPM");
	if(metadata.GetBPM()) {
		SFB::CFString str(nullptr, CFSTR("%@"), metadata.GetBPM());

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TBPM", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}

	// Rating
	tag->removeFrames("POPM");
	CFNumberRef rating = metadata.GetRating();
	if(rating) {
		TagLib::ID3v2::PopularimeterFrame *frame = new TagLib::ID3v2::PopularimeterFrame();

		int i;
		if(CFNumberGetValue(rating, kCFNumberIntType, &i)) {
			frame->setRating(i);
			tag->addFrame(frame);
		}
		else {
			delete frame;
		}
	}

	// Track number and total tracks
	tag->removeFrames("TRCK");
	CFNumberRef trackNumber	= metadata.GetTrackNumber();
	CFNumberRef trackTotal	= metadata.GetTrackTotal();
	if(trackNumber && trackTotal) {
		SFB::CFString str(nullptr, CFSTR("%@/%@"), trackNumber, trackTotal);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}
	else if(trackNumber) {
		SFB::CFString str(nullptr, CFSTR("%@"), trackNumber);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}
	else if(trackTotal) {
		SFB::CFString str(nullptr, CFSTR("/%@"), trackTotal);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}

	// Compilation
	// iTunes uses the TCMP frame for this, which isn't in the standard, but we'll use it for compatibility
	tag->removeFrames("TCMP");
	if(metadata.GetCompilation()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TCMP", TagLib::String::Latin1);
		frame->setText(CFBooleanGetValue(metadata.GetCompilation()) ? "1" : "0");
		tag->addFrame(frame);
	}

	// Disc number and total discs
	tag->removeFrames("TPOS");
	CFNumberRef discNumber	= metadata.GetDiscNumber();
	CFNumberRef discTotal	= metadata.GetDiscTotal();
	if(discNumber && discTotal) {
		SFB::CFString str(nullptr, CFSTR("%@/%@"), discNumber, discTotal);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}
	else if(discNumber) {
		SFB::CFString str(nullptr, CFSTR("%@"), discNumber);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}
	else if(discTotal) {
		SFB::CFString str(nullptr, CFSTR("/%@"), discTotal);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}

	// Lyrics
	tag->removeFrames("USLT");
	if(metadata.GetLyrics()) {
		auto frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(TagLib::String::UTF8);
		frame->setText(TagLib::StringFromCFString(metadata.GetLyrics()));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSRC");
	if(metadata.GetISRC()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSRC", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(metadata.GetISRC()));
		tag->addFrame(frame);
	}

	// MusicBrainz
	auto musicBrainzReleaseIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "MusicBrainz Album Id");
	if(nullptr != musicBrainzReleaseIDFrame)
		tag->removeFrame(musicBrainzReleaseIDFrame);

	CFStringRef musicBrainzReleaseID = metadata.GetMusicBrainzReleaseID();
	if(musicBrainzReleaseID) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("MusicBrainz Album Id");
		frame->setText(TagLib::StringFromCFString(musicBrainzReleaseID));
		tag->addFrame(frame);
	}


	auto musicBrainzRecordingIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Track Id");
	if(nullptr != musicBrainzRecordingIDFrame)
		tag->removeFrame(musicBrainzRecordingIDFrame);

	CFStringRef musicBrainzRecordingID = metadata.GetMusicBrainzRecordingID();
	if(musicBrainzRecordingID) {
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("MusicBrainz Track Id");
		frame->setText(TagLib::StringFromCFString(musicBrainzRecordingID));
		tag->addFrame(frame);
	}

	// Sorting and grouping
	tag->removeFrames("TSOT");
	if(metadata.GetTitleSortOrder()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOT", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromCFString(metadata.GetTitleSortOrder()));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSOA");
	if(metadata.GetAlbumTitleSortOrder()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOA", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromCFString(metadata.GetAlbumTitleSortOrder()));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSOP");
	if(metadata.GetArtistSortOrder()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOP", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromCFString(metadata.GetArtistSortOrder()));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSO2");
	if(metadata.GetAlbumArtistSortOrder()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSO2", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromCFString(metadata.GetAlbumArtistSortOrder()));
		tag->addFrame(frame);
	}

	tag->removeFrames("TSOC");
	if(metadata.GetComposerSortOrder()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TSOC", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromCFString(metadata.GetComposerSortOrder()));
		tag->addFrame(frame);
	}

	tag->removeFrames("TIT1");
	if(metadata.GetGrouping()) {
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TIT1", TagLib::String::UTF8);
		frame->setText(TagLib::StringFromCFString(metadata.GetGrouping()));
		tag->addFrame(frame);
	}

	// ReplayGain
	CFNumberRef trackGain = metadata.GetReplayGainTrackGain();
	CFNumberRef trackPeak = metadata.GetReplayGainTrackPeak();
	CFNumberRef albumGain = metadata.GetReplayGainAlbumGain();
	CFNumberRef albumPeak = metadata.GetReplayGainAlbumPeak();

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

	if(trackGain) {
		SFB::CFString str(CreateStringFromNumberWithFormat(trackGain, kCFNumberDoubleType, CFSTR("%+2.2f dB")));

		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_track_gain");
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}

	if(trackPeak) {
		SFB::CFString str(CreateStringFromNumberWithFormat(trackPeak, kCFNumberDoubleType, CFSTR("%1.8f dB")));

		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_track_peak");
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}

	if(albumGain) {
		SFB::CFString str(CreateStringFromNumberWithFormat(albumGain, kCFNumberDoubleType, CFSTR("%+2.2f dB")));

		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_album_gain");
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}

	if(albumPeak) {
		SFB::CFString str(CreateStringFromNumberWithFormat(albumPeak, kCFNumberDoubleType, CFSTR("%1.8f dB")));

		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_album_peak");
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
	}

	// Also write the RVA2 frames
	tag->removeFrames("RVA2");
	if(trackGain) {
		auto relativeVolume = new TagLib::ID3v2::RelativeVolumeFrame();

		float f;
		CFNumberGetValue(trackGain, kCFNumberFloatType, &f);

		relativeVolume->setIdentification(TagLib::String("track", TagLib::String::Latin1));
		relativeVolume->setVolumeAdjustment(f, TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);

		tag->addFrame(relativeVolume);
	}

	if(albumGain) {
		auto relativeVolume = new TagLib::ID3v2::RelativeVolumeFrame();

		float f;
		CFNumberGetValue(albumGain, kCFNumberFloatType, &f);

		relativeVolume->setIdentification(TagLib::String("album", TagLib::String::Latin1));
		relativeVolume->setVolumeAdjustment(f, TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);

		tag->addFrame(relativeVolume);
	}

	// Album art
	if(setAlbumArt) {
		tag->removeFrames("APIC");

		for(auto attachedPicture : metadata.GetAttachedPictures()) {
			SFB::CGImageSource imageSource(CGImageSourceCreateWithData(attachedPicture->GetData(), nullptr));
			if(!imageSource)
				continue;

			TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;

			// Convert the image's UTI into a MIME type
			SFB::CFString mimeType(UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType));
			if(mimeType)
				frame->setMimeType(TagLib::StringFromCFString(mimeType));

			frame->setPicture(TagLib::ByteVector((const char *)CFDataGetBytePtr(attachedPicture->GetData()), (size_t)CFDataGetLength(attachedPicture->GetData())));
			frame->setType((TagLib::ID3v2::AttachedPictureFrame::Type)attachedPicture->GetType());
			if(attachedPicture->GetDescription())
				frame->setDescription(TagLib::StringFromCFString(attachedPicture->GetDescription()));
			tag->addFrame(frame);
		}
	}

	return true;
}
