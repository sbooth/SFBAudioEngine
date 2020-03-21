/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <taglib/mp4coverart.h>

#import "SFBAudioMetadata+TagLibMP4Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (TagLibMP4Tag)

- (void)addMetadataFromTagLibMP4Tag:(const TagLib::MP4::Tag *)tag
{
	NSParameterAssert(tag != nil);

	// Add the basic tags not specific to MP4
	[self addMetadataFromTagLibTag:tag];

	if(tag->contains("aART"))
		self.albumArtist = [NSString stringWithUTF8String:tag->item("aART").toString().toCString(true)];
	if(tag->contains("\251wrt"))
		self.composer = [NSString stringWithUTF8String:tag->item("\251wrt").toString().toCString(true)];
	if(tag->contains("\251day"))
		self.releaseDate = [NSString stringWithUTF8String:tag->item("\251day").toString().toCString(true)];

	if(tag->contains("trkn")) {
		auto track = tag->item("trkn").toIntPair();
		if(track.first)
			self.trackNumber = [NSNumber numberWithInt:track.first];
		if(track.second)
			self.trackTotal = [NSNumber numberWithInt:track.second];
	}
	if(tag->contains("disk")) {
		auto disc = tag->item("disk").toIntPair();
		if(disc.first)
			self.discNumber = [NSNumber numberWithInt:disc.first];
		if(disc.second)
			self.discTotal = [NSNumber numberWithInt:disc.second];
	}
	if(tag->contains("cpil")) {
		if(tag->item("cpil").toBool())
			self.compilation = [NSNumber numberWithBool:YES];
	}
	if(tag->contains("tmpo")) {
		auto bpm = tag->item("tmpo").toInt();
		if(bpm)
			self.bpm = [NSNumber numberWithInt:bpm];
	}
	if(tag->contains("\251lyr"))
		self.lyrics = [NSString stringWithUTF8String:tag->item("\251lyr").toString().toCString(true)];

	// Sorting
	if(tag->contains("sonm"))
		self.titleSortOrder = [NSString stringWithUTF8String:tag->item("sonm").toString().toCString(true)];
	if(tag->contains("soal"))
		self.albumTitleSortOrder = [NSString stringWithUTF8String:tag->item("soal").toString().toCString(true)];
	if(tag->contains("soar"))
		self.artistSortOrder = [NSString stringWithUTF8String:tag->item("soar").toString().toCString(true)];
	if(tag->contains("soaa"))
		self.albumArtistSortOrder = [NSString stringWithUTF8String:tag->item("soaa").toString().toCString(true)];
	if(tag->contains("soco"))
		self.composerSortOrder = [NSString stringWithUTF8String:tag->item("soco").toString().toCString(true)];

	if(tag->contains("\251grp"))
		self.lyrics = [NSString stringWithUTF8String:tag->item("\251grp").toString().toCString(true)];

	// Album art
	if(tag->contains("covr")) {
		auto art = tag->item("covr").toCoverArtList();
		for(auto iter : art) {
			NSData *data = [NSData dataWithBytes:iter.data().data() length:iter.data().size()];
			[self attachPicture:[[SFBAttachedPicture alloc] initWithImageData:data]];
		}
	}

	// MusicBrainz
	if(tag->contains("---:com.apple.iTunes:MusicBrainz Album Id"))
		self.musicBrainzReleaseID = [NSString stringWithUTF8String:tag->item("---:com.apple.iTunes:MusicBrainz Album Id").toString().toCString(true)];

	if(tag->contains("---:com.apple.iTunes:MusicBrainz Track Id"))
		self.musicBrainzRecordingID = [NSString stringWithUTF8String:tag->item("---:com.apple.iTunes:MusicBrainz Track Id").toString().toCString(true)];

	// ReplayGain
	if(tag->contains("---:com.apple.iTunes:replaygain_reference_loudness")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_reference_loudness").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainReferenceLoudness = [NSNumber numberWithFloat:f];
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_track_gain")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_track_gain").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainTrackGain = [NSNumber numberWithFloat:f];
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_track_peak")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_track_peak").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainTrackPeak = [NSNumber numberWithFloat:f];
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_album_gain")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_album_gain").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainAlbumGain = [NSNumber numberWithFloat:f];
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_album_peak")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_album_peak").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainAlbumPeak = [NSNumber numberWithFloat:f];
	}
}

@end
