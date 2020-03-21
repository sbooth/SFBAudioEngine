/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "Base64Utilities.h"
#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (TagLibXiphComment)

- (void)addMetadataFromTagLibXiphComment:(const TagLib::Ogg::XiphComment *)tag
{
	NSParameterAssert(tag != nil);

	NSMutableDictionary *additionalMetadata = [NSMutableDictionary dictionary];

	for(auto it : tag->fieldListMap()) {
		// According to the Xiph comment specification keys should only contain a limited subset of ASCII, but UTF-8 is a safer choice
		NSString *key = [NSString stringWithUTF8String:it.first.toCString(true)];

		// Vorbis allows multiple comments with the same key, but this isn't supported by AudioMetadata
		NSString *value = [NSString stringWithUTF8String:it.second.front().toCString(true)];

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
		else if([key caseInsensitiveCompare:@"METADATA_BLOCK_PICTURE"] == NSOrderedSame) {
			// Handle embedded pictures
			for(auto blockIterator : it.second) {
				auto encodedBlock = blockIterator.data(TagLib::String::UTF8);

				// Decode the Base-64 encoded data
				auto decodedBlock = TagLib::DecodeBase64(encodedBlock);

				// Create the picture
				TagLib::FLAC::Picture picture;
				picture.parse(decodedBlock);

				NSData *imageData = [NSData dataWithBytes:picture.data().data() length:picture.data().size()];

				NSString *description = nil;
				if(!picture.description().isEmpty())
					description = [NSString stringWithUTF8String:picture.description().toCString(true)];

				[self attachPicture:[[SFBAttachedPicture alloc] initWithImageData:imageData
																			 type:(SFBAttachedPictureType)picture.type()
																	  description:description]];
			}
		}
		// Put all unknown tags into the additional metadata
		else
			[additionalMetadata setObject:value forKey:key];
	}
}

@end
