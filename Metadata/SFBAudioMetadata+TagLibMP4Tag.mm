/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <taglib/mp4coverart.h>

#import "SFBAudioMetadata+TagLibMP4Tag.h"

#import "CFWrapper.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "TagLibStringUtilities.h"

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
			self.trackNumber = @(track.first);
		if(track.second)
			self.trackTotal = @(track.second);
	}
	if(tag->contains("disk")) {
		auto disc = tag->item("disk").toIntPair();
		if(disc.first)
			self.discNumber = @(disc.first);
		if(disc.second)
			self.discTotal = @(disc.second);
	}
	if(tag->contains("cpil")) {
		if(tag->item("cpil").toBool())
			self.compilation = @(YES);
	}
	if(tag->contains("tmpo")) {
		auto bpm = tag->item("tmpo").toInt();
		if(bpm)
			self.bpm = @(bpm);
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
			self.replayGainReferenceLoudness = @(f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_track_gain")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_track_gain").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainTrackGain = @(f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_track_peak")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_track_peak").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainTrackPeak = @(f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_album_gain")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_album_gain").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainAlbumGain = @(f);
	}

	if(tag->contains("---:com.apple.iTunes:replaygain_album_peak")) {
		auto s = tag->item("---:com.apple.iTunes:replaygain_album_peak").toString();
		float f;
		if(::sscanf(s.toCString(), "%f", &f) == 1)
			self.replayGainAlbumPeak = @(f);
	}
}

@end

namespace {

	void SetMP4Item(TagLib::MP4::Tag *tag, const char *key, NSString *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing item with this name
		tag->removeItem(key);

		if(value)
			tag->setItem(key, TagLib::MP4::Item(TagLib::StringFromNSString(value)));
	}

	void SetMP4ItemInt(TagLib::MP4::Tag *tag, const char *key, NSNumber *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing item with this name
		tag->removeItem(key);

		if(value)
			tag->setItem(key, TagLib::MP4::Item(value.intValue));
	}

	void SetMP4ItemIntPair(TagLib::MP4::Tag *tag, const char *key, NSNumber *valueOne, NSNumber *valueTwo)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		// Remove the existing item with this name
		tag->removeItem(key);

		if(valueOne || valueTwo)
			tag->setItem(key, TagLib::MP4::Item(valueOne.intValue, valueTwo.intValue));
	}

	void SetMP4ItemBoolean(TagLib::MP4::Tag *tag, const char *key, NSNumber *value)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		if(!value)
			tag->removeItem(key);
		else
			tag->setItem(key, TagLib::MP4::Item((int)value.boolValue));
	}

	void SetMP4ItemDoubleWithFormat(TagLib::MP4::Tag *tag, const char *key, NSNumber *value, NSString *format = nil)
	{
		assert(nullptr != tag);
		assert(nullptr != key);

		SetMP4Item(tag, key, value ? [NSString stringWithFormat:(format ?: @"%f"), value.doubleValue] : nil);
	}

}

void SFB::Audio::SetMP4TagFromMetadata(SFBAudioMetadata *metadata, TagLib::MP4::Tag *tag, bool setAlbumArt)
{
	NSCParameterAssert(metadata != nil);
	assert(nullptr != tag);

	SetMP4Item(tag, "\251nam", metadata.title);
	SetMP4Item(tag, "\251ART", metadata.artist);
	SetMP4Item(tag, "\251ALB", metadata.albumTitle);
	SetMP4Item(tag, "aART", metadata.albumArtist);
	SetMP4Item(tag, "\251gen", metadata.genre);
	SetMP4Item(tag, "\251wrt", metadata.composer);
	SetMP4Item(tag, "\251cmt", metadata.comment);
	SetMP4Item(tag, "\251day", metadata.releaseDate);

	SetMP4ItemIntPair(tag, "trkn", metadata.trackNumber, metadata.trackTotal);
	SetMP4ItemIntPair(tag, "disk", metadata.discNumber, metadata.discTotal);

	SetMP4ItemBoolean(tag, "cpil", metadata.compilation);

	SetMP4ItemInt(tag, "tmpo", metadata.bpm);

	SetMP4Item(tag, "\251lyr", metadata.lyrics);

	// Sorting
	SetMP4Item(tag, "sonm", metadata.titleSortOrder);
	SetMP4Item(tag, "soal", metadata.albumTitleSortOrder);
	SetMP4Item(tag, "soar", metadata.artistSortOrder);
	SetMP4Item(tag, "soaa", metadata.albumArtistSortOrder);
	SetMP4Item(tag, "soco", metadata.composerSortOrder);

	SetMP4Item(tag, "\251grp", metadata.grouping);

	// MusicBrainz
	SetMP4Item(tag, "---:com.apple.iTunes:MusicBrainz Album Id", metadata.musicBrainzReleaseID);
	SetMP4Item(tag, "---:com.apple.iTunes:MusicBrainz Track Id", metadata.musicBrainzRecordingID);

	// ReplayGain info
	SetMP4ItemDoubleWithFormat(tag, "---:com.apple.iTunes:replaygain_reference_loudness", metadata.replayGainReferenceLoudness, @"%2.1f dB");
	SetMP4ItemDoubleWithFormat(tag, "---:com.apple.iTunes:replaygain_track_gain", metadata.replayGainTrackGain, @"%2.2f dB");
	SetMP4ItemDoubleWithFormat(tag, "---:com.apple.iTunes:replaygain_track_peak", metadata.replayGainTrackPeak, @"%1.8f dB");
	SetMP4ItemDoubleWithFormat(tag, "---:com.apple.iTunes:replaygain_album_gain", metadata.replayGainAlbumGain, @"%2.2f dB");
	SetMP4ItemDoubleWithFormat(tag, "---:com.apple.iTunes:replaygain_album_peak", metadata.replayGainAlbumPeak, @"%1.8f dB");

	if(setAlbumArt) {
		auto list = TagLib::MP4::CoverArtList();
		for(SFBAttachedPicture *attachedPicture in metadata.attachedPictures) {
			SFB::CGImageSource imageSource(CGImageSourceCreateWithData((__bridge CFDataRef)attachedPicture.imageData, nullptr));
			if(!imageSource)
				continue;

			// Convert the image's UTI into a MIME type
			NSString *mimeType = (__bridge_transfer NSString *)UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
			auto type = TagLib::MP4::CoverArt::CoverArt::Unknown;
			if(mimeType) {
				if(UTTypeEqual(kUTTypeBMP, (__bridge CFStringRef)mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::BMP;
				else if(UTTypeEqual(kUTTypePNG, (__bridge CFStringRef)mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::PNG;
				else if(UTTypeEqual(kUTTypeGIF, (__bridge CFStringRef)mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::GIF;
				else if(UTTypeEqual(kUTTypeJPEG, (__bridge CFStringRef)mimeType))
					type = TagLib::MP4::CoverArt::CoverArt::JPEG;
			}

			auto picture = TagLib::MP4::CoverArt(type, TagLib::ByteVector((const char *)attachedPicture.imageData.bytes, (size_t)attachedPicture.imageData.length));
			list.append(picture);
		}

		tag->setItem("covr", list);
	}
}
