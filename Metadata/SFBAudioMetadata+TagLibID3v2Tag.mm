/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <taglib/attachedpictureframe.h>
#include <taglib/id3v2frame.h>
#include <taglib/popularimeterframe.h>
#include <taglib/relativevolumeframe.h>
#include <taglib/textidentificationframe.h>

#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (TagLibID3v2Tag)

- (void)addMetadataFromTagLibID3v2Tag:(const TagLib::ID3v2::Tag *)tag
{
	NSParameterAssert(tag != nil);

	// Add the basic tags not specific to ID3v2
	[self addMetadataFromTagLibTag:tag];

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

		self.releaseDate = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
	}

	// Extract composer if present
	frameList = tag->frameListMap()["TCOM"];
	if(!frameList.isEmpty())
		self.composer = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	// Extract album artist
	frameList = tag->frameListMap()["TPE2"];
	if(!frameList.isEmpty())
		self.albumArtist = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	// BPM
	frameList = tag->frameListMap()["TBPM"];
	if(!frameList.isEmpty()) {
		bool ok = false;
		int BPM = frameList.front()->toString().toInt(&ok);
		if(ok)
			self.bpm = [NSNumber numberWithInt:BPM];
	}

	// Rating
	TagLib::ID3v2::PopularimeterFrame *popularimeter = nullptr;
	frameList = tag->frameListMap()["POPM"];
	if(!frameList.isEmpty() && nullptr != (popularimeter = dynamic_cast<TagLib::ID3v2::PopularimeterFrame *>(frameList.front())))
		self.rating = [NSNumber numberWithInt:popularimeter->rating()];

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
				self.trackNumber = [NSNumber numberWithInt:trackNum];

			int trackTotal = s.substr(pos + 1).toInt(&ok);
			if(ok)
				self.trackTotal = [NSNumber numberWithInt:trackTotal];
		}
		else if(s.length()) {
			int trackNum = s.toInt(&ok);
			if(ok)
				self.trackNumber = [NSNumber numberWithInt:trackNum];
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
				self.trackNumber = [NSNumber numberWithInt:discNum];

			int discTotal = s.substr(pos + 1).toInt(&ok);
			if(ok)
				self.trackNumber = [NSNumber numberWithInt:discTotal];
		}
		else if(s.length()) {
			int discNum = s.toInt(&ok);
			if(ok)
				self.trackNumber = [NSNumber numberWithInt:discNum];
		}
	}

	// Lyrics
	frameList = tag->frameListMap()["USLT"];
	if(!frameList.isEmpty())
		self.lyrics = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	// Extract compilation if present (iTunes TCMP tag)
	frameList = tag->frameListMap()["TCMP"];
	if(!frameList.isEmpty())
		// It seems that the presence of this frame indicates a compilation
		self.compilation = [NSNumber numberWithBool:YES];

	frameList = tag->frameListMap()["TSRC"];
	if(!frameList.isEmpty())
		self.isrc = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	// MusicBrainz
	auto musicBrainzReleaseIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Album Id");
	if(musicBrainzReleaseIDFrame)
		self.musicBrainzReleaseID = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	auto musicBrainzRecordingIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Track Id");
	if(musicBrainzRecordingIDFrame)
		self.musicBrainzRecordingID = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	// Sorting and grouping
	frameList = tag->frameListMap()["TSOT"];
	if(!frameList.isEmpty())
		self.titleSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	frameList = tag->frameListMap()["TSOA"];
	if(!frameList.isEmpty())
		self.albumTitleSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	frameList = tag->frameListMap()["TSOP"];
	if(!frameList.isEmpty())
		self.artistSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	frameList = tag->frameListMap()["TSO2"];
	if(!frameList.isEmpty())
		self.albumArtistSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	frameList = tag->frameListMap()["TSOC"];
	if(!frameList.isEmpty())
		self.composerSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

	frameList = tag->frameListMap()["TIT1"];
	if(!frameList.isEmpty())
		self.grouping = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];

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
		NSString *s = [NSString stringWithUTF8String:trackGainFrame->fieldList().back().toCString(true)];
		self.replayGainTrackGain = [NSNumber numberWithDouble:s.doubleValue];
		self.replayGainReferenceLoudness = [NSNumber numberWithDouble:89.0];

		foundReplayGain = true;
	}

	if(!trackPeakFrame)
		trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_track_peak");
	if(trackPeakFrame) {
		NSString *s = [NSString stringWithUTF8String:trackPeakFrame->fieldList().back().toCString(true)];
		self.replayGainTrackPeak = [NSNumber numberWithDouble:s.doubleValue];
	}

	if(!albumGainFrame)
		albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_gain");
	if(albumGainFrame) {
		NSString *s = [NSString stringWithUTF8String:albumGainFrame->fieldList().back().toCString(true)];
		self.replayGainAlbumGain = [NSNumber numberWithDouble:s.doubleValue];
		self.replayGainReferenceLoudness = [NSNumber numberWithDouble:89.0];

		foundReplayGain = true;
	}

	if(!albumPeakFrame)
		albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag), "replaygain_album_peak");
	if(albumPeakFrame) {
		NSString *s = [NSString stringWithUTF8String:albumPeakFrame->fieldList().back().toCString(true)];
		self.replayGainAlbumPeak = [NSNumber numberWithDouble:s.doubleValue];
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
					self.replayGainTrackGain = [NSNumber numberWithFloat:volumeAdjustment];
			}
			else if(TagLib::String("album", TagLib::String::Latin1) == relativeVolume->identification()) {
				if((int)volumeAdjustment)
					self.replayGainAlbumGain = [NSNumber numberWithFloat:volumeAdjustment];
			}
			// Fall back to track gain if identification is not specified
			else {
				if((int)volumeAdjustment)
					self.replayGainTrackGain = [NSNumber numberWithFloat:volumeAdjustment];
			}
		}
	}

	// Extract album art if present
	for(auto it : tag->frameListMap()["APIC"]) {
		TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(it);
		if(frame) {
			NSData *imageData = [NSData dataWithBytes:frame->picture().data() length:frame->picture().size()];
			NSString *description = nil;
			if(!frame->description().isEmpty())
				description = [NSString stringWithUTF8String:frame->description().toCString(true)];

			SFBAttachedPicture *picture = [[SFBAttachedPicture alloc] initWithImageData:imageData
																				   type:(SFBAttachedPictureType)frame->type()
																			description:description];
			[self attachPicture:picture];
		}
	}
}

@end
