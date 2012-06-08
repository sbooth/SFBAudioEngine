/*
 *  Copyright (C) 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include "AddID3v2TagToDictionary.h"
#include "AddTagToDictionary.h"
#include "AudioMetadata.h"
#include "TagLibStringUtilities.h"
#include "CFDictionaryUtilities.h"

bool
AddID3v2TagToDictionary(CFMutableDictionaryRef dictionary, std::vector<AttachedPicture *>& attachedPictures, const TagLib::ID3v2::Tag *tag)
{
	if(nullptr == dictionary || nullptr == tag)
		return false;

	attachedPictures.clear();

	// Add the basic tags not specific to ID3v2
	AddTagToDictionary(dictionary, tag);

	// Extract composer if present
	auto frameList = tag->frameListMap()["TCOM"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataComposerKey, frameList.front()->toString());
	
	// Extract album artist
	frameList = tag->frameListMap()["TPE2"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataAlbumArtistKey, frameList.front()->toString());
	
	// BPM
	frameList = tag->frameListMap()["TBPM"];
	if(!frameList.isEmpty()) {
		bool ok = false;
		int BPM = frameList.front()->toString().toInt(&ok);
		if(ok)
			AddIntToDictionary(dictionary, kMetadataBPMKey, BPM);
	}

	// Rating
	TagLib::ID3v2::PopularimeterFrame *popularimeter = nullptr;
	frameList = tag->frameListMap()["POPM"];
	if(!frameList.isEmpty() && nullptr != (popularimeter = dynamic_cast<TagLib::ID3v2::PopularimeterFrame *>(frameList.front())))
		AddIntToDictionary(dictionary, kMetadataRatingKey, popularimeter->rating());

	// Extract total tracks if present
	frameList = tag->frameListMap()["TRCK"];
	if(!frameList.isEmpty()) {
		// Split the tracks at '/'
		TagLib::String s = frameList.front()->toString();

		bool ok;
		int pos = s.find("/", 0);		
		if(-1 != pos) {
			int trackNum = s.substr(0, pos).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, kMetadataTrackNumberKey, trackNum);

			int trackTotal = s.substr(pos + 1).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, kMetadataTrackTotalKey, trackTotal);
		}
		else if(s.length()) {
			int trackNum = s.toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, kMetadataTrackNumberKey, trackNum);
		}
	}
	
	// Extract disc number and total discs
	frameList = tag->frameListMap()["TPOS"];
	if(!frameList.isEmpty()) {
		// Split the tracks at '/'
		TagLib::String s = frameList.front()->toString();

		bool ok;
		int pos = s.find("/", 0);
		if(-1 != pos) {
			int discNum = s.substr(0, pos).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, kMetadataDiscNumberKey, discNum);

			int discTotal = s.substr(pos + 1).toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, kMetadataDiscTotalKey, discTotal);
		}
		else if(s.length()) {
			int discNum = s.toInt(&ok);
			if(ok)
				AddIntToDictionary(dictionary, kMetadataDiscNumberKey, discNum);
		}
	}

	// Lyrics
	frameList = tag->frameListMap()["USLT"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataLyricsKey, frameList.front()->toString());

	// Extract compilation if present (iTunes TCMP tag)
	frameList = tag->frameListMap()["TCMP"];
	if(!frameList.isEmpty())
		// It seems that the presence of this frame indicates a compilation
		CFDictionarySetValue(dictionary, kMetadataCompilationKey, kCFBooleanTrue);

	frameList = tag->frameListMap()["TSRC"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataISRCKey, frameList.front()->toString());

	// Sorting and grouping
	frameList = tag->frameListMap()["TSOT"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataTitleSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSOA"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataAlbumTitleSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSOP"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataArtistSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSO2"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataAlbumArtistSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TSOC"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataComposerSortOrderKey, frameList.front()->toString());

	frameList = tag->frameListMap()["TIT1"];
	if(!frameList.isEmpty())
		TagLib::AddStringToCFDictionary(dictionary, kMetadataGroupingKey, frameList.front()->toString());

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
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, trackGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = nullptr;

		AddDoubleToDictionary(dictionary, kReplayGainTrackGainKey, num);
		AddDoubleToDictionary(dictionary, kReplayGainReferenceLoudnessKey, 89.0);
		
		foundReplayGain = true;
	}
	
	if(!trackPeakFrame)
		trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_track_peak");
	if(trackPeakFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, trackPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = nullptr;
		
		AddDoubleToDictionary(dictionary, kReplayGainTrackPeakKey, num);
	}
	
	if(!albumGainFrame)
		albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_gain");
	if(albumGainFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, albumGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = nullptr;
		
		AddDoubleToDictionary(dictionary, kReplayGainAlbumGainKey, num);
		AddDoubleToDictionary(dictionary, kReplayGainReferenceLoudnessKey, 89.0);
		
		foundReplayGain = true;
	}
	
	if(!albumPeakFrame)
		albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_peak");
	if(albumPeakFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, albumPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = nullptr;

		AddDoubleToDictionary(dictionary, kReplayGainAlbumPeakKey, num);
	}
	
	// If nothing found check for RVA2 frame
	if(!foundReplayGain) {
		frameList = tag->frameListMap()["RVA2"];
		
		for(auto frameIterator : tag->frameListMap()["RVA2"]) {
			TagLib::ID3v2::RelativeVolumeFrame *relativeVolume = dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame *>(frameIterator);
			if(!relativeVolume)
				continue;
			
			if(TagLib::String("track", TagLib::String::Latin1) == relativeVolume->identification()) {
				// Attempt to use the master volume if present
				auto channels		= relativeVolume->channels();
				auto channelType	= TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;
				
				// Fall back on whatever else exists in the frame
				if(!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume))
					channelType = channels.front();
				
				float volumeAdjustment = relativeVolume->volumeAdjustment(channelType);
				
				if(volumeAdjustment)
					AddFloatToDictionary(dictionary, kReplayGainTrackGainKey, volumeAdjustment);
			}
			else if(TagLib::String("album", TagLib::String::Latin1) == relativeVolume->identification()) {
				// Attempt to use the master volume if present
				auto channels		= relativeVolume->channels();
				auto channelType	= TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;
				
				// Fall back on whatever else exists in the frame
				if(!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume))
					channelType = channels.front();
				
				float volumeAdjustment = relativeVolume->volumeAdjustment(channelType);
				
				if(volumeAdjustment)
					AddFloatToDictionary(dictionary, kReplayGainAlbumGainKey, volumeAdjustment);
			}
			// Fall back to track gain if identification is not specified
			else {
				// Attempt to use the master volume if present
				auto channels		= relativeVolume->channels();
				auto channelType	= TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;
				
				// Fall back on whatever else exists in the frame
				if(!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume))
					channelType = channels.front();
				
				float volumeAdjustment = relativeVolume->volumeAdjustment(channelType);
				
				if(volumeAdjustment)
					AddFloatToDictionary(dictionary, kReplayGainAlbumGainKey, volumeAdjustment);
			}
		}			
	}
	
	// Extract album art if present
	for(auto it : tag->frameListMap()["APIC"]) {
		TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(it);
		if(frame) {
			CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(frame->picture().data()), frame->picture().size());
			
			CFStringRef description = nullptr;
			if(!frame->description().isNull())
				description = CFStringCreateWithCString(kCFAllocatorDefault, frame->description().toCString(true), kCFStringEncodingUTF8);
			
			AttachedPicture *p = new AttachedPicture(data, static_cast<AttachedPicture::Type>(frame->type()), description);
			attachedPictures.push_back(p);
			
			if(data)
				CFRelease(data), data = nullptr;
			
			if(description)
				CFRelease(description), description = nullptr;
		}
	}

	return true;
}
