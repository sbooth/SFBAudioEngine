/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/relativevolumeframe.h>
#include <taglib/popularimeterframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <ApplicationServices/ApplicationServices.h>

#include "AudioMetadata.h"
#include "SetID3v2TagFromMetadata.h"
#include "TagLibStringUtilities.h"

// If type and format don't match, watch out!
static CFStringRef
CreateStringFromNumberWithFormat(CFNumberRef	value,
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

			return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, nullptr == format ? CFSTR("%f") : format, d);
		}
			
			// Everything else
		default:
			return CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@"), value);
	}

	return nullptr;
}


bool
SetID3v2TagFromMetadata(const AudioMetadata& metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt)
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
	int year = 0;
	if(metadata.GetReleaseDate())
		year = CFStringGetIntValue(metadata.GetReleaseDate());
	tag->setYear(year);
	
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
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@"), metadata.GetBPM());

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TBPM", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
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
	}

	// Track number and total tracks
	tag->removeFrames("TRCK");
	CFNumberRef trackNumber	= metadata.GetTrackNumber();
	CFNumberRef trackTotal	= metadata.GetTrackTotal();
	if(trackNumber && trackTotal) {
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@/%@"), trackNumber, trackTotal);

		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
	}
	else if(trackNumber) {		
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@"), trackNumber);
		
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
	}
	else if(trackTotal) {		
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("/%@"), trackTotal);
		
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
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
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@/%@"), discNumber, discTotal);
		
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
		
		CFRelease(str), str = nullptr;
	}
	else if(discNumber) {		
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("%@"), discNumber);
		
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
		
		CFRelease(str), str = nullptr;
	}
	else if(discTotal) {		
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("/%@"), discTotal);
		
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
		frame->setText(TagLib::StringFromCFString(str));
		tag->addFrame(frame);
		
		CFRelease(str), str = nullptr;
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
		CFStringRef str = CreateStringFromNumberWithFormat(trackGain, kCFNumberDoubleType, CFSTR("%+2.2f dB"));
		
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_track_gain");
		frame->setText(TagLib::StringFromCFString(str));		
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
	}
	
	if(trackPeak) {		
		CFStringRef str = CreateStringFromNumberWithFormat(trackPeak, kCFNumberDoubleType, CFSTR("%1.8f dB"));
		
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
		frame->setDescription("replaygain_track_peak");
		frame->setText(TagLib::StringFromCFString(str));		
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
	}
	
	if(albumGain) {
		CFStringRef str = CreateStringFromNumberWithFormat(albumGain, kCFNumberDoubleType, CFSTR("%+2.2f dB"));

		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();		
		frame->setDescription("replaygain_album_gain");
		frame->setText(TagLib::StringFromCFString(str));		
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
	}
	
	if(albumPeak) {
		CFStringRef str = CreateStringFromNumberWithFormat(albumPeak, kCFNumberDoubleType, CFSTR("%1.8f dB"));
		
		auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();		
		frame->setDescription("replaygain_album_peak");
		frame->setText(TagLib::StringFromCFString(str));		
		tag->addFrame(frame);

		CFRelease(str), str = nullptr;
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
		// For some reason for(auto frame : tag->frameList("APIC")) crashes (clang C++11 bug?)
		auto frames = tag->frameList("APIC");
		for(auto frame : frames)
			tag->removeFrame(frame);
		
		for(auto attachedPicture : metadata.GetAttachedPictures()) {
			CGImageSourceRef imageSource = CGImageSourceCreateWithData(attachedPicture->GetData(), nullptr);
			if(nullptr == imageSource)
				continue;

			TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;

			// Convert the image's UTI into a MIME type
			CFStringRef mimeType = UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
			if(mimeType) {
				frame->setMimeType(TagLib::StringFromCFString(mimeType));
				CFRelease(mimeType), mimeType = nullptr;
			}

			frame->setPicture(TagLib::ByteVector(reinterpret_cast<const char *>(CFDataGetBytePtr(attachedPicture->GetData())), static_cast<TagLib::uint>(CFDataGetLength(attachedPicture->GetData()))));
			frame->setType(static_cast<TagLib::ID3v2::AttachedPictureFrame::Type>(attachedPicture->GetType()));
			if(attachedPicture->GetDescription())
				frame->setDescription(TagLib::StringFromCFString(attachedPicture->GetDescription()));
			tag->addFrame(frame);

			CFRelease(imageSource), imageSource = nullptr;
		}
	}

	return true;
}
