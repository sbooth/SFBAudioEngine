/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+TagLibAPETag.h"
#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (TagLibAPETag)

- (void)addMetadataFromTagLibAPETag:(const TagLib::APE::Tag *)tag
{
	NSParameterAssert(tag != nil);

	if(tag->isEmpty())
		return;

	NSMutableDictionary *additionalMetadata = [NSMutableDictionary dictionary];

	for(auto iterator : tag->itemListMap()) {
		auto item = iterator.second;

		if(item.isEmpty())
			continue;

		if(TagLib::APE::Item::Text == item.type()) {
			NSString *key = [NSString stringWithUTF8String:item.key().toCString(true)];
			NSString *value = [NSString stringWithUTF8String:item.toString().toCString(true)];

			if([key caseInsensitiveCompare:@"ALBUM"] == NSOrderedSame)
				self.albumTitle = value;
			else if([key caseInsensitiveCompare:@"ARTIST"] == NSOrderedSame)
				self.artist = value;
			else if([key caseInsensitiveCompare:@"ALBUMARTIST"] == NSOrderedSame)
				self.albumArtist = value;
			else if([key caseInsensitiveCompare:@"COMPOSER"] == NSOrderedSame)
				self.composer = value;
			else if([key caseInsensitiveCompare:@"GENRE"] == NSOrderedSame)
				self.genre = value;
			else if([key caseInsensitiveCompare:@"DATE"] == NSOrderedSame)
				self.releaseDate = value;
			else if([key caseInsensitiveCompare:@"DESCRIPTION"] == NSOrderedSame)
				self.comment = value;
			else if([key caseInsensitiveCompare:@"TITLE"] == NSOrderedSame)
				self.title = value;
			else if([key caseInsensitiveCompare:@"TRACKNUMBER"] == NSOrderedSame)
				self.trackNumber = @(value.integerValue);
			else if([key caseInsensitiveCompare:@"TRACKTOTAL"] == NSOrderedSame)
				self.trackTotal = @(value.integerValue);
			else if([key caseInsensitiveCompare:@"COMPILATION"] == NSOrderedSame)
				self.compilation = @(value.boolValue);
			else if([key caseInsensitiveCompare:@"DISCNUMBER"] == NSOrderedSame)
				self.discNumber = @(value.integerValue);
			else if([key caseInsensitiveCompare:@"DISCTOTAL"] == NSOrderedSame)
				self.discTotal = @(value.integerValue);
			else if([key caseInsensitiveCompare:@"LYRICS"] == NSOrderedSame)
				self.lyrics = value;
			else if([key caseInsensitiveCompare:@"BPM"] == NSOrderedSame)
				self.bpm = @(value.integerValue);
			else if([key caseInsensitiveCompare:@"RATING"] == NSOrderedSame)
				self.rating = @(value.integerValue);
			else if([key caseInsensitiveCompare:@"ISRC"] == NSOrderedSame)
				self.isrc = value;
			else if([key caseInsensitiveCompare:@"MCN"] == NSOrderedSame)
				self.mcn = value;
			else if([key caseInsensitiveCompare:@"MUSICBRAINZ_ALBUMID"] == NSOrderedSame)
				self.musicBrainzReleaseID = value;
			else if([key caseInsensitiveCompare:@"MUSICBRAINZ_TRACKID"] == NSOrderedSame)
				self.musicBrainzRecordingID = value;
			else if([key caseInsensitiveCompare:@"TITLESORT"] == NSOrderedSame)
				self.titleSortOrder = value;
			else if([key caseInsensitiveCompare:@"ALBUMTITLESORT"] == NSOrderedSame)
				self.albumTitleSortOrder = value;
			else if([key caseInsensitiveCompare:@"ARTISTSORT"] == NSOrderedSame)
				self.artistSortOrder = value;
			else if([key caseInsensitiveCompare:@"ALBUMARTISTSORT"] == NSOrderedSame)
				self.albumArtistSortOrder = value;
			else if([key caseInsensitiveCompare:@"COMPOSERSORT"] == NSOrderedSame)
				self.composerSortOrder = value;
			else if([key caseInsensitiveCompare:@"GROUPING"] == NSOrderedSame)
				self.grouping = value;
			else if([key caseInsensitiveCompare:@"REPLAYGAIN_REFERENCE_LOUDNESS"] == NSOrderedSame)
				self.replayGainReferenceLoudness = @(value.doubleValue);
			else if([key caseInsensitiveCompare:@"REPLAYGAIN_TRACK_GAIN"] == NSOrderedSame)
				self.replayGainTrackGain = @(value.doubleValue);
			else if([key caseInsensitiveCompare:@"REPLAYGAIN_TRACK_PEAK"] == NSOrderedSame)
				self.replayGainTrackPeak = @(value.doubleValue);
			else if([key caseInsensitiveCompare:@"REPLAYGAIN_ALBUM_GAIN"] == NSOrderedSame)
				self.replayGainAlbumGain = @(value.doubleValue);
			else if([key caseInsensitiveCompare:@"REPLAYGAIN_ALBUM_PEAK"] == NSOrderedSame)
				self.replayGainAlbumPeak = @(value.doubleValue);
			// Put all unknown tags into the additional metadata
			else
				[additionalMetadata setObject:value forKey:key];
		}
		else if(TagLib::APE::Item::Binary == item.type()) {
			NSString *key = [NSString stringWithUTF8String:item.key().toCString(true)];

			// From http://www.hydrogenaudio.org/forums/index.php?showtopic=40603&view=findpost&p=504669
			/*
			 <length> 32 bit
			 <flags with binary bit set> 32 bit
			 <field name> "Cover Art (Front)"|"Cover Art (Back)"
			 0x00
			 <description> UTF-8 string (needs to be a file name to be recognized by AudioShell - meh)
			 0x00
			 <cover data> binary
			 */
			if([key caseInsensitiveCompare:@"Cover Art (Front)"] == NSOrderedSame || [key caseInsensitiveCompare:@"Cover Art (Back)"] == NSOrderedSame) {
				auto binaryData = item.binaryData();
				size_t pos = binaryData.find('\0');
				if(TagLib::ByteVector::npos() != pos && 3 < binaryData.size()) {
					NSData *imageData = [NSData dataWithBytes:binaryData.mid(pos + 1).data() length:(binaryData.size() - pos - 1)];
					SFBAttachedPictureType type = [key caseInsensitiveCompare:@"Cover Art (Front)"] == NSOrderedSame ? SFBAttachedPictureTypeFrontCover : SFBAttachedPictureTypeBackCover;
					NSString *description = [NSString stringWithUTF8String:TagLib::String(binaryData.mid(0, pos), TagLib::String::UTF8).toCString(true)];
					[self attachPicture:[[SFBAttachedPicture alloc] initWithImageData:imageData type:type description:description]];
				}
			}
		}
	}

	if(additionalMetadata.count)
		self.additionalMetadata = additionalMetadata;
}

@end
