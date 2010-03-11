/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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
#include <taglib/textidentificationframe.h>

#include "AudioEngineDefines.h"
#include "AudioMetadata.h"

#include "SetMetadataFromID3v2Tag.h"

bool
SetMetadataFromID3v2Tag(AudioMetadata *metadata, TagLib::ID3v2::Tag *tag)
{
	assert(NULL != metadata);
	assert(NULL != tag);
	
	// Album title
	if(!tag->album().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->album().toCString(true), kCFStringEncodingUTF8);
		metadata->SetAlbumTitle(str);
		CFRelease(str), str = NULL;
	}
	
	// Artist
	if(!tag->artist().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->artist().toCString(true), kCFStringEncodingUTF8);
		metadata->SetArtist(str);
		CFRelease(str), str = NULL;
	}
	
	// Genre
	if(!tag->genre().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->genre().toCString(true), kCFStringEncodingUTF8);
		metadata->SetGenre(str);
		CFRelease(str), str = NULL;
	}

	// Year
	if(tag->year()) {
		CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), tag->year());
		metadata->SetReleaseDate(str);
		CFRelease(str), str = NULL;
	}
	
	// Comment
	if(!tag->comment().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->comment().toCString(true), kCFStringEncodingUTF8);
		metadata->SetComment(str);
		CFRelease(str), str = NULL;
	}
	
	// Track title
	if(!tag->title().isNull()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, tag->title().toCString(true), kCFStringEncodingUTF8);
		metadata->SetTitle(str);
		CFRelease(str), str = NULL;
	}
	
	// Track number
	if(tag->track()) {
		int trackNum = tag->track();
		CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackNum);
		metadata->SetTrackNumber(num);
		CFRelease(num), num = NULL;
	}
	
	// Extract composer if present
	TagLib::ID3v2::FrameList frameList = tag->frameListMap()["TCOM"];
	if(!frameList.isEmpty()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, frameList.front()->toString().toCString(true), kCFStringEncodingUTF8);
		metadata->SetComposer(str);
		CFRelease(str), str = NULL;
	}
	
	// Extract album artist
	frameList = tag->frameListMap()["TPE2"];
	if(!frameList.isEmpty()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, frameList.front()->toString().toCString(true), kCFStringEncodingUTF8);
		metadata->SetAlbumArtist(str);
		CFRelease(str), str = NULL;
	}
	
	// BPM
	frameList = tag->frameListMap()["TBPM"];
	if(!frameList.isEmpty()) {
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
			metadata->SetTrackNumber(num);
			CFRelease(num), num = NULL;			

			num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackTotal);
			metadata->SetTrackTotal(num);
			CFRelease(num), num = NULL;			
		}
		else if(s.length()) {
			int trackNum = s.toInt();
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &trackNum);
			metadata->SetTrackNumber(num);
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
			metadata->SetDiscNumber(num);
			CFRelease(num), num = NULL;			
			
			num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &discTotal);
			metadata->SetDiscTotal(num);
			CFRelease(num), num = NULL;			
		}
		else if(s.length()) {
			int discNum = s.toInt();
			CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &discNum);
			metadata->SetDiscNumber(num);
			CFRelease(num), num = NULL;			
		}
	}

	// Lyrics
	frameList = tag->frameListMap()["USLT"];
	if(!frameList.isEmpty()) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, frameList.front()->toString().toCString(true), kCFStringEncodingUTF8);
		metadata->SetLyrics(str);
		CFRelease(str), str = NULL;
	}
	
	// Extract album art if present
	TagLib::ID3v2::AttachedPictureFrame *picture = NULL;
	frameList = tag->frameListMap()["APIC"];
	if(!frameList.isEmpty() && NULL != (picture = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frameList.front()))) {
		TagLib::ByteVector pictureBytes = picture->picture();
		CFDataRef pictureData = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(pictureBytes.data()), pictureBytes.size());
		metadata->SetFrontCoverArt(pictureData);
		CFRelease(pictureData), pictureData = NULL;
	}

	// Extract compilation if present (iTunes TCMP tag)
	frameList = tag->frameListMap()["TCMP"];
	if(!frameList.isEmpty())
		// It seems that the presence of this frame indicates a compilation
		metadata->SetCompilation(kCFBooleanTrue);
	
	// ReplayGain
	bool foundReplayGain = false;
	
	// Preference is TXXX frames, RVA2 frame, then LAME header
	TagLib::ID3v2::UserTextIdentificationFrame *trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "REPLAYGAIN_TRACK_GAIN");
	TagLib::ID3v2::UserTextIdentificationFrame *trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "REPLAYGAIN_TRACK_PEAK");
	TagLib::ID3v2::UserTextIdentificationFrame *albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "REPLAYGAIN_ALBUM_GAIN");
	TagLib::ID3v2::UserTextIdentificationFrame *albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "REPLAYGAIN_ALBUM_PEAK");
	
	if(!trackGainFrame)
		trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_track_gain");
	if(trackGainFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, trackGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;

		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		metadata->SetReplayGainTrackGain(number);
		CFRelease(number), number = NULL;

		num = 89;
		number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		metadata->SetReplayGainReferenceLoudness(number);
		CFRelease(number), number = NULL;
		
		foundReplayGain = true;
	}
	
	if(!trackPeakFrame)
		trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_track_peak");
	if(trackPeakFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, trackPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;
		
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		metadata->SetReplayGainTrackPeak(number);
		CFRelease(number), number = NULL;
	}
	
	if(!albumGainFrame)
		albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_album_gain");
	if(albumGainFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, albumGainFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;
		
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		metadata->SetReplayGainAlbumGain(number);
		CFRelease(number), number = NULL;
		
		num = 89;
		number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		metadata->SetReplayGainReferenceLoudness(number);
		CFRelease(number), number = NULL;
		
		foundReplayGain = true;
	}
	
	if(!albumPeakFrame)
		albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_album_peak");
	if(albumPeakFrame) {
		CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, albumPeakFrame->fieldList().back().toCString(true), kCFStringEncodingUTF8);
		double num = CFStringGetDoubleValue(str);
		CFRelease(str), str = NULL;
		
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &num);
		metadata->SetReplayGainAlbumPeak(number);
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
					metadata->SetReplayGainTrackGain(number);
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
					metadata->SetReplayGainAlbumGain(number);
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
					metadata->SetReplayGainAlbumGain(number);
					CFRelease(number), number = NULL;
					
					foundReplayGain = true;
				}
			}
		}			
	}
	
	return true;
}
