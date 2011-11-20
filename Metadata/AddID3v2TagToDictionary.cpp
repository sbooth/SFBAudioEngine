/*
 *  Copyright (C) 2010, 2011 Stephen F. Booth <me@sbooth.org>
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
#include "AudioMetadata.h"

bool
AddID3v2TagToDictionary(CFMutableDictionaryRef dictionary, const TagLib::ID3v2::Tag *tag)
{
	if(NULL == dictionary || NULL == tag)
		return false;
	
	// Album title
	if(!tag->album().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->album().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataAlbumTitleKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Artist
	if(!tag->artist().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->artist().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataArtistKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Genre
	if(!tag->genre().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->genre().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataGenreKey, str);
		CFRelease(str), str = NULL;
	}

	// Year
	if(tag->year()) {
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), tag->year());
		CFDictionarySetValue(dictionary, kMetadataReleaseDateKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Comment
	if(!tag->comment().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->comment().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataCommentKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Track title
	if(!tag->title().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->title().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataTitleKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Track number
	if(tag->track()) {
		int trackNum = tag->track();
		CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackNum);
		CFDictionarySetValue(dictionary, kMetadataTrackNumberKey, num);
		CFRelease(num), num = NULL;
	}
	
	// Extract composer if present
	TagLib::ID3v2::FrameList frameList = tag->frameListMap()["TCOM"];
	if(!frameList.isEmpty()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, frameList.front()->toString().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataComposerKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Extract album artist
	frameList = tag->frameListMap()["TPE2"];
	if(!frameList.isEmpty()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, frameList.front()->toString().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataAlbumArtistKey, str);
		CFRelease(str), str = NULL;
	}
	
	// BPM
	frameList = tag->frameListMap()["TBPM"];
	if(!frameList.isEmpty()) {
		bool ok = false;
		int BPM = frameList.front()->toString().toInt(&ok);
		if(ok) {
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &BPM);
			CFDictionarySetValue(dictionary, kMetadataBPMKey, num);
			CFRelease(num), num = NULL;
		}
	}

	// Rating
	TagLib::ID3v2::PopularimeterFrame *popularimeter = NULL;
	frameList = tag->frameListMap()["POPM"];
	if(!frameList.isEmpty() && NULL != (popularimeter = dynamic_cast<TagLib::ID3v2::PopularimeterFrame *>(frameList.front()))) {
		int rating = popularimeter->rating();
		CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &rating);
		CFDictionarySetValue(dictionary, kMetadataRatingKey, num);
		CFRelease(num), num = NULL;
	}

	// Extract total tracks if present
	frameList = tag->frameListMap()["TRCK"];
	if(!frameList.isEmpty()) {
		// Split the tracks at '/'
		TagLib::String s = frameList.front()->toString();

		int pos = s.find("/", 0);
		
		if(-1 != pos) {
			int trackNum = s.substr(0, pos).toInt();
			int trackTotal = s.substr(pos + 1).toInt();
			
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackNum);
			CFDictionarySetValue(dictionary, kMetadataTrackNumberKey, num);
			CFRelease(num), num = NULL;			

			num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackTotal);
			CFDictionarySetValue(dictionary, kMetadataTrackTotalKey, num);
			CFRelease(num), num = NULL;			
		}
		else if(s.length()) {
			int trackNum = s.toInt();
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackNum);
			CFDictionarySetValue(dictionary, kMetadataTrackNumberKey, num);
			CFRelease(num), num = NULL;			
		}
	}
	
	// Extract disc number and total discs
	frameList = tag->frameListMap()["TPOS"];
	if(!frameList.isEmpty()) {
		// Split the tracks at '/'
		TagLib::String s = frameList.front()->toString();
		
		int pos = s.find("/", 0);
		
		if(-1 != pos) {
			int discNum = s.substr(0, pos).toInt();
			int discTotal = s.substr(pos + 1).toInt();
			
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &discNum);
			CFDictionarySetValue(dictionary, kMetadataDiscNumberKey, num);
			CFRelease(num), num = NULL;			
			
			num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &discTotal);
			CFDictionarySetValue(dictionary, kMetadataDiscTotalKey, num);
			CFRelease(num), num = NULL;			
		}
		else if(s.length()) {
			int discNum = s.toInt();
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &discNum);
			CFDictionarySetValue(dictionary, kMetadataDiscNumberKey, num);
			CFRelease(num), num = NULL;			
		}
	}

	// Lyrics
	frameList = tag->frameListMap()["USLT"];
	if(!frameList.isEmpty()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, frameList.front()->toString().toCString(true), kCFStringEncodingUTF8);
		CFDictionarySetValue(dictionary, kMetadataLyricsKey, str);
		CFRelease(str), str = NULL;
	}
	
	// Extract album art if present
	frameList = tag->frameListMap()["APIC"];
	for(TagLib::ID3v2::FrameList::ConstIterator it = frameList.begin(); it != frameList.end(); ++it) {
		TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(*it);
		if(frame) {
			switch(frame->type()) {
					// Front cover art
				case TagLib::ID3v2::AttachedPictureFrame::FrontCover:
				{
					TagLib::ByteVector pictureBytes = frame->picture();
					CFDataRef pictureData = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(pictureBytes.data()), pictureBytes.size());
					CFDictionarySetValue(dictionary, kAlbumArtFrontCoverKey, pictureData);
					CFRelease(pictureData), pictureData = NULL;
					break;
				}

					// TODO: Other artwork types will be handled in the future
				default:
					break;
			}
		}
	}

	// Extract compilation if present (iTunes TCMP tag)
	frameList = tag->frameListMap()["TCMP"];
	if(!frameList.isEmpty())
		// It seems that the presence of this frame indicates a compilation
		CFDictionarySetValue(dictionary, kMetadataCompilationKey, kCFBooleanTrue);
	
	// ReplayGain
	bool foundReplayGain = false;
	
	// Preference is TXXX frames, RVA2 frame, then LAME header
	TagLib::ID3v2::UserTextIdentificationFrame *trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_TRACK_GAIN");
	TagLib::ID3v2::UserTextIdentificationFrame *trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_TRACK_PEAK");
	TagLib::ID3v2::UserTextIdentificationFrame *albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_ALBUM_GAIN");
	TagLib::ID3v2::UserTextIdentificationFrame *albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "REPLAYGAIN_ALBUM_PEAK");
	
	if(!trackGainFrame)
		trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_track_gain");
	if(trackGainFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, trackGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;

		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		CFDictionarySetValue(dictionary, kReplayGainTrackGainKey, number);
		CFRelease(number), number = NULL;

		num = 89;
		number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		CFDictionarySetValue(dictionary, kReplayGainReferenceLoudnessKey, number);
		CFRelease(number), number = NULL;
		
		foundReplayGain = true;
	}
	
	if(!trackPeakFrame)
		trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_track_peak");
	if(trackPeakFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, trackPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;
		
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		CFDictionarySetValue(dictionary, kReplayGainTrackPeakKey, number);
		CFRelease(number), number = NULL;
	}
	
	if(!albumGainFrame)
		albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_gain");
	if(albumGainFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, albumGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;
		
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		CFDictionarySetValue(dictionary, kReplayGainAlbumGainKey, number);
		CFRelease(number), number = NULL;
		
		num = 89;
		number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		CFDictionarySetValue(dictionary, kReplayGainReferenceLoudnessKey, number);
		CFRelease(number), number = NULL;
		
		foundReplayGain = true;
	}
	
	if(!albumPeakFrame)
		albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_peak");
	if(albumPeakFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, albumPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;
		
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		CFDictionarySetValue(dictionary, kReplayGainAlbumPeakKey, number);
		CFRelease(number), number = NULL;
	}
	
	// If nothing found check for RVA2 frame
	if(!foundReplayGain) {
		frameList = tag->frameListMap()["RVA2"];
		
		TagLib::ID3v2::FrameList::Iterator frameIterator;
		for(frameIterator = frameList.begin(); frameIterator != frameList.end(); ++frameIterator) {
			TagLib::ID3v2::RelativeVolumeFrame *relativeVolume = dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame *>(*frameIterator);
			if(!relativeVolume)
				continue;
			
			if(TagLib::String("track", TagLib::String::Latin1) == relativeVolume->identification()) {
				// Attempt to use the master volume if present
				TagLib::List<TagLib::ID3v2::RelativeVolumeFrame::ChannelType>	channels		= relativeVolume->channels();
				TagLib::ID3v2::RelativeVolumeFrame::ChannelType					channelType		= TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;
				
				// Fall back on whatever else exists in the frame
				if(!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume))
					channelType = channels.front();
				
				float volumeAdjustment = relativeVolume->volumeAdjustment(channelType);
				
				if(volumeAdjustment) {
					CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &volumeAdjustment);
					CFDictionarySetValue(dictionary, kReplayGainTrackGainKey, number);
					CFRelease(number), number = NULL;

					foundReplayGain = true;
				}
			}
			else if(TagLib::String("album", TagLib::String::Latin1) == relativeVolume->identification()) {
				// Attempt to use the master volume if present
				TagLib::List<TagLib::ID3v2::RelativeVolumeFrame::ChannelType>	channels		= relativeVolume->channels();
				TagLib::ID3v2::RelativeVolumeFrame::ChannelType					channelType		= TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;
				
				// Fall back on whatever else exists in the frame
				if(!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume))
					channelType = channels.front();
				
				float volumeAdjustment = relativeVolume->volumeAdjustment(channelType);
				
				if(volumeAdjustment) {
					CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &volumeAdjustment);
					CFDictionarySetValue(dictionary, kReplayGainAlbumGainKey, number);
					CFRelease(number), number = NULL;
					
					foundReplayGain = true;
				}
			}
			// Fall back to track gain if identification is not specified
			else {
				// Attempt to use the master volume if present
				TagLib::List<TagLib::ID3v2::RelativeVolumeFrame::ChannelType>	channels		= relativeVolume->channels();
				TagLib::ID3v2::RelativeVolumeFrame::ChannelType					channelType		= TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;
				
				// Fall back on whatever else exists in the frame
				if(!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume))
					channelType = channels.front();
				
				float volumeAdjustment = relativeVolume->volumeAdjustment(channelType);
				
				if(volumeAdjustment) {
					CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &volumeAdjustment);
					CFDictionarySetValue(dictionary, kReplayGainAlbumGainKey, number);
					CFRelease(number), number = NULL;
					
					foundReplayGain = true;
				}
			}
		}			
	}
	
	return true;
}
