/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata.h"

// Key names for the metadata dictionary
SFBAudioMetadataKey const SFBAudioMetadataKeyTitle							= @"Title";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitle						= @"Album Title";
SFBAudioMetadataKey const SFBAudioMetadataKeyArtist							= @"Artist";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtist					= @"Album Artist";
SFBAudioMetadataKey const SFBAudioMetadataKeyGenre							= @"Genre";
SFBAudioMetadataKey const SFBAudioMetadataKeyComposer						= @"Composer";
SFBAudioMetadataKey const SFBAudioMetadataKeyReleaseDate					= @"Date";
SFBAudioMetadataKey const SFBAudioMetadataKeyCompilation					= @"Compilation";
SFBAudioMetadataKey const SFBAudioMetadataKeyTrackNumber					= @"Track Number";
SFBAudioMetadataKey const SFBAudioMetadataKeyTrackTotal						= @"Track Total";
SFBAudioMetadataKey const SFBAudioMetadataKeyDiscNumber						= @"Disc Number";
SFBAudioMetadataKey const SFBAudioMetadataKeyDiscTotal						= @"Disc Total";
SFBAudioMetadataKey const SFBAudioMetadataKeyLyrics							= @"Lyrics";
SFBAudioMetadataKey const SFBAudioMetadataKeyBPM							= @"BPM";
SFBAudioMetadataKey const SFBAudioMetadataKeyRating							= @"Rating";
SFBAudioMetadataKey const SFBAudioMetadataKeyComment						= @"Comment";
SFBAudioMetadataKey const SFBAudioMetadataKeyISRC							= @"ISRC";
SFBAudioMetadataKey const SFBAudioMetadataKeyMCN							= @"MCN";
SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzReleaseID			= @"MusicBrainz Release ID";
SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzRecordingID			= @"MusicBrainz Recording ID";

SFBAudioMetadataKey const SFBAudioMetadataKeyTitleSortOrder					= @"Title Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitleSortOrder			= @"Album Title Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyArtistSortOrder				= @"Artist Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtistSortOrder			= @"Album Artist Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyComposerSortOrder				= @"Composer Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyGenreSortOrder					= @"Genre Sort Order";

SFBAudioMetadataKey const SFBAudioMetadataKeyGrouping						= @"Grouping";

SFBAudioMetadataKey const SFBAudioMetadataKeyAdditionalMetadata				= @"Additional Metadata";

SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainReferenceLoudness	= @"Replay Gain Reference Loudness";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackGain			= @"Replay Gain Track Gain";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackPeak			= @"Replay Gain Track Peak";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumGain			= @"Replay Gain Album Gain";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumPeak			= @"Replay Gain Album Peak";

SFBAudioMetadataKey const SFBAudioMetadataKeyAttachedPictures				= @"Attached Pictures";

@interface SFBAudioMetadata ()
{
@private
	NSMutableDictionary *_metadata;
	NSMutableSet *_pictures;
}
+ (id)sharedKeySet;
@end

@implementation SFBAudioMetadata

static id _sharedKeySet;

+ (id)sharedKeySet
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_sharedKeySet = [NSDictionary sharedKeySetForKeys:@[
			SFBAudioMetadataKeyTitle,
			SFBAudioMetadataKeyAlbumTitle,
			SFBAudioMetadataKeyArtist,
			SFBAudioMetadataKeyAlbumArtist,
			SFBAudioMetadataKeyGenre,
			SFBAudioMetadataKeyComposer,
			SFBAudioMetadataKeyReleaseDate,
			SFBAudioMetadataKeyCompilation,
			SFBAudioMetadataKeyTrackNumber,
			SFBAudioMetadataKeyTrackTotal,
			SFBAudioMetadataKeyDiscNumber,
			SFBAudioMetadataKeyDiscTotal,
			SFBAudioMetadataKeyLyrics,
			SFBAudioMetadataKeyBPM,
			SFBAudioMetadataKeyRating,
			SFBAudioMetadataKeyComment,
			SFBAudioMetadataKeyISRC,
			SFBAudioMetadataKeyMCN,
			SFBAudioMetadataKeyMusicBrainzReleaseID,
			SFBAudioMetadataKeyMusicBrainzRecordingID,

			SFBAudioMetadataKeyTitleSortOrder,
			SFBAudioMetadataKeyAlbumTitleSortOrder,
			SFBAudioMetadataKeyArtistSortOrder,
			SFBAudioMetadataKeyAlbumArtistSortOrder,
			SFBAudioMetadataKeyComposerSortOrder,
			SFBAudioMetadataKeyGenreSortOrder,

			SFBAudioMetadataKeyGrouping,

			SFBAudioMetadataKeyAdditionalMetadata,

			SFBAudioMetadataKeyReplayGainReferenceLoudness,
			SFBAudioMetadataKeyReplayGainTrackGain,
			SFBAudioMetadataKeyReplayGainTrackPeak,
			SFBAudioMetadataKeyReplayGainAlbumGain,
			SFBAudioMetadataKeyReplayGainAlbumPeak,

			SFBAudioMetadataKeyAttachedPictures
		]];
	});
	return _sharedKeySet;
}

- (instancetype)init
{
	if((self = [super init])) {
		_metadata = [NSMutableDictionary dictionaryWithSharedKeySet:[SFBAudioMetadata sharedKeySet]];
		_pictures = [NSMutableSet set];
	}
	return self;
}

- (instancetype)initWithDictionaryRepresentation:(NSDictionary *)dictionaryRepresentation
{
	if((self = [self init]))
		[self setFromDictionaryRepresentation:dictionaryRepresentation];
	return self;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone
{
#pragma unused(zone)
	SFBAudioMetadata *result = [[[self class] alloc] init];
	result->_metadata = [_metadata mutableCopy];
	result->_pictures = [_pictures mutableCopy];
	return result;
}

#pragma mark Basic Metadata

- (NSString *)title
{
	return [_metadata objectForKey:SFBAudioMetadataKeyTitle];
}

- (void)setTitle:(NSString *)title
{
	[_metadata setObject:title forKey:SFBAudioMetadataKeyTitle];
}

- (NSString *)albumTitle
{
	return [_metadata objectForKey:SFBAudioMetadataKeyAlbumTitle];
}

- (void)setAlbumTitle:(NSString *)albumTitle
{
	[_metadata setObject:albumTitle forKey:SFBAudioMetadataKeyAlbumTitle];
}

- (NSString *)artist
{
	return [_metadata objectForKey:SFBAudioMetadataKeyArtist];
}

- (void)setArtist:(NSString *)artist
{
	[_metadata setObject:artist forKey:SFBAudioMetadataKeyArtist];
}

- (NSString *)albumArtist
{
	return [_metadata objectForKey:SFBAudioMetadataKeyAlbumArtist];
}

- (void)setAlbumArtist:(NSString *)albumArtist
{
	[_metadata setObject:albumArtist forKey:SFBAudioMetadataKeyAlbumArtist];
}

- (NSString *)genre
{
	return [_metadata objectForKey:SFBAudioMetadataKeyGenre];
}

- (void)setGenre:(NSString *)genre
{
	[_metadata setObject:genre forKey:SFBAudioMetadataKeyGenre];
}

- (NSString *)composer
{
	return [_metadata objectForKey:SFBAudioMetadataKeyComposer];
}

- (void)setComposer:(NSString *)composer
{
	[_metadata setObject:composer forKey:SFBAudioMetadataKeyComposer];
}

- (NSString *)releaseDate
{
	return [_metadata objectForKey:SFBAudioMetadataKeyReleaseDate];
}

- (void)setReleaseDate:(NSString *)releaseDate
{
	[_metadata setObject:releaseDate forKey:SFBAudioMetadataKeyReleaseDate];
}

- (NSNumber *)compilation
{
	return [_metadata objectForKey:SFBAudioMetadataKeyCompilation];
}

- (void)setCompilation:(NSNumber *)compilation
{
	[_metadata setObject:compilation forKey:SFBAudioMetadataKeyCompilation];
}

- (NSNumber *)trackNumber
{
	return [_metadata objectForKey:SFBAudioMetadataKeyTrackNumber];
}

- (void)setTrackNumber:(NSNumber *)trackNumber
{
	[_metadata setObject:trackNumber forKey:SFBAudioMetadataKeyTrackNumber];
}

- (NSNumber *)trackTotal
{
	return [_metadata objectForKey:SFBAudioMetadataKeyTrackTotal];
}

- (void)setTrackTotal:(NSNumber *)trackTotal
{
	[_metadata setObject:trackTotal forKey:SFBAudioMetadataKeyTrackTotal];
}

- (NSNumber *)discNumber
{
	return [_metadata objectForKey:SFBAudioMetadataKeyDiscNumber];
}

- (void)setDiscNumber:(NSNumber *)discNumber
{
	[_metadata setObject:discNumber forKey:SFBAudioMetadataKeyDiscNumber];
}

- (NSNumber *)discTotal
{
	return [_metadata objectForKey:SFBAudioMetadataKeyDiscTotal];
}

- (void)setDiscTotal:(NSNumber *)discTotal
{
	[_metadata setObject:discTotal forKey:SFBAudioMetadataKeyDiscTotal];
}

- (NSString *)lyrics
{
	return [_metadata objectForKey:SFBAudioMetadataKeyLyrics];
}

- (void)setLyrics:(NSString *)lyrics
{
	[_metadata setObject:lyrics forKey:SFBAudioMetadataKeyLyrics];
}

- (NSNumber *)bpm
{
	return [_metadata objectForKey:SFBAudioMetadataKeyBPM];
}

- (void)setBpm:(NSNumber *)bpm
{
	[_metadata setObject:bpm forKey:SFBAudioMetadataKeyBPM];
}

- (NSNumber *)rating
{
	return [_metadata objectForKey:SFBAudioMetadataKeyRating];
}

- (void)setRating:(NSNumber *)rating
{
	[_metadata setObject:rating forKey:SFBAudioMetadataKeyRating];
}

- (NSString *)comment
{
	return [_metadata objectForKey:SFBAudioMetadataKeyComment];
}

- (void)setComment:(NSString *)comment
{
	[_metadata setObject:comment forKey:SFBAudioMetadataKeyComment];
}

- (NSString *)isrc
{
	return [_metadata objectForKey:SFBAudioMetadataKeyISRC];
}

- (void)setIsrc:(NSString *)isrc
{
	[_metadata setObject:isrc forKey:SFBAudioMetadataKeyISRC];
}

- (NSString *)mcn
{
	return [_metadata objectForKey:SFBAudioMetadataKeyMCN];
}

- (void)setMcn:(NSString *)mcn
{
	[_metadata setObject:mcn forKey:SFBAudioMetadataKeyMCN];
}

- (NSString *)musicBrainzReleaseID
{
	return [_metadata objectForKey:SFBAudioMetadataKeyMusicBrainzReleaseID];
}

- (void)setMusicBrainzReleaseID:(NSString *)musicBrainzReleaseID
{
	[_metadata setObject:musicBrainzReleaseID forKey:SFBAudioMetadataKeyMusicBrainzReleaseID];
}

- (NSString *)musicBrainzRecordingID
{
	return [_metadata objectForKey:SFBAudioMetadataKeyMusicBrainzRecordingID];
}

- (void)setMusicBrainzRecordingID:(NSString *)musicBrainzRecordingID
{
	[_metadata setObject:musicBrainzRecordingID forKey:SFBAudioMetadataKeyMusicBrainzRecordingID];
}

#pragma mark Sorting Metadata

- (NSString *)titleSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataKeyTitleSortOrder];
}
- (void)setTitleSortOrder:(NSString *)titleSortOrder
{
	[_metadata setObject:titleSortOrder forKey:SFBAudioMetadataKeyTitleSortOrder];
}

- (NSString *)albumTitleSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataKeyAlbumTitleSortOrder];
}

- (void)setAlbumTitleSortOrder:(NSString *)albumTitleSortOrder
{
	[_metadata setObject:albumTitleSortOrder forKey:SFBAudioMetadataKeyAlbumTitleSortOrder];
}

- (NSString *)artistSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataKeyArtistSortOrder];
}

- (void)setArtistSortOrder:(NSString *)artistSortOrder
{
	[_metadata setObject:artistSortOrder forKey:SFBAudioMetadataKeyArtistSortOrder];
}

- (NSString *)albumArtistSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataKeyAlbumArtistSortOrder];
}

- (void)setAlbumArtistSortOrder:(NSString *)albumArtistSortOrder
{
	[_metadata setObject:albumArtistSortOrder forKey:SFBAudioMetadataKeyAlbumArtistSortOrder];
}

- (NSString *)composerSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataKeyComposerSortOrder];
}

- (void)setComposerSortOrder:(NSString *)composerSortOrder
{
	[_metadata setObject:composerSortOrder forKey:SFBAudioMetadataKeyComposerSortOrder];
}

- (NSString *)genreSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataKeyGenreSortOrder];
}

- (void)setGenreSortOrder:(NSString *)genreSortOrder
{
	[_metadata setObject:genreSortOrder forKey:SFBAudioMetadataKeyGenreSortOrder];
}

#pragma mark Grouping Metadata

- (NSString *)grouping
{
	return [_metadata objectForKey:SFBAudioMetadataKeyGrouping];
}

- (void)setGrouping:(NSString *)grouping
{
	[_metadata setObject:grouping forKey:SFBAudioMetadataKeyGrouping];
}

#pragma mark Additional Metadata

- (NSDictionary *)additionalMetadata
{
	return [_metadata objectForKey:SFBAudioMetadataKeyAdditionalMetadata];
}

- (void)setAdditionalMetadata:(NSDictionary *)additionalMetadata
{
	[_metadata setObject:[additionalMetadata copy] forKey:SFBAudioMetadataKeyAdditionalMetadata];
}

#pragma mark Replay Gain Metadata

- (NSNumber *)replayGainReferenceLoudness
{
	return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainReferenceLoudness];
}

- (void)setReplayGainReferenceLoudness:(NSNumber *)replayGainReferenceLoudness
{
	[_metadata setObject:replayGainReferenceLoudness forKey:SFBAudioMetadataKeyReplayGainReferenceLoudness];
}

- (NSNumber *)replayGainTrackGain
{
	return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainTrackGain];
}

- (void)setReplayGainTrackGain:(NSNumber *)replayGainTrackGain
{
	[_metadata setObject:replayGainTrackGain forKey:SFBAudioMetadataKeyReplayGainTrackGain];
}

- (NSNumber *)replayGainTrackPeak
{
	return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainTrackPeak];
}

- (void)setReplayGainTrackPeak:(NSNumber *)replayGainTrackPeak
{
	[_metadata setObject:replayGainTrackPeak forKey:SFBAudioMetadataKeyReplayGainTrackPeak];
}

- (NSNumber *)replayGainAlbumGain
{
	return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainAlbumGain];
}

- (void)setReplayGainAlbumGain:(NSNumber *)replayGainAlbumGain
{
	[_metadata setObject:replayGainAlbumGain forKey:SFBAudioMetadataKeyReplayGainAlbumGain];
}

- (NSNumber *)replayGainAlbumPeak
{
	return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainAlbumPeak];
}

- (void)setReplayGainAlbumPeak:(NSNumber *)replayGainAlbumPeak
{
	[_metadata setObject:replayGainAlbumPeak forKey:SFBAudioMetadataKeyReplayGainAlbumPeak];
}

#pragma mark Metadata Utilities

- (void) copyMetadataOfKind:(SFBAudioMetadataKind)kind from:(SFBAudioMetadata *)metadata
{
	if(kind & SFBAudioMetadataKindBasic) {
		self.title = metadata.title;
		self.albumTitle = metadata.albumTitle;
		self.artist = metadata.artist;
		self.albumArtist = metadata.albumArtist;
		self.genre = metadata.genre;
		self.composer = metadata.composer;
		self.releaseDate = metadata.releaseDate;
		self.compilation = metadata.compilation;
		self.trackNumber = metadata.trackNumber;
		self.trackTotal = metadata.trackTotal;
		self.discNumber = metadata.discNumber;
		self.discTotal = metadata.discTotal;
		self.lyrics = metadata.lyrics;
		self.bpm = metadata.bpm;
		self.rating = metadata.rating;
		self.comment = metadata.comment;
		self.isrc = metadata.isrc;
		self.mcn = metadata.mcn;
		self.musicBrainzReleaseID = metadata.musicBrainzReleaseID;
		self.musicBrainzRecordingID = metadata.musicBrainzRecordingID;
	}

	if(kind & SFBAudioMetadataKindSorting) {
		self.titleSortOrder = metadata.titleSortOrder;
		self.albumTitleSortOrder = metadata.albumTitleSortOrder;
		self.artistSortOrder = metadata.artistSortOrder;
		self.albumArtistSortOrder = metadata.albumArtistSortOrder;
		self.composerSortOrder = metadata.composerSortOrder;
		self.genreSortOrder = metadata.genreSortOrder;
	}

	if(kind & SFBAudioMetadataKindGrouping)
		self.grouping = metadata.grouping;

	if(kind & SFBAudioMetadataKindAdditional)
		self.additionalMetadata = metadata.additionalMetadata;

	if(kind & SFBAudioMetadataKindReplayGain) {
		self.replayGainReferenceLoudness = metadata.replayGainReferenceLoudness;
		self.replayGainTrackGain = metadata.replayGainTrackGain;
		self.replayGainTrackPeak = metadata.replayGainTrackPeak;
		self.replayGainAlbumGain = metadata.replayGainAlbumGain;
		self.replayGainAlbumPeak = metadata.replayGainAlbumPeak;
	}
}

- (void) copyMetadataFrom:(SFBAudioMetadata *)metadata
{
	[self copyMetadataOfKind:(SFBAudioMetadataKindBasic | SFBAudioMetadataKindSorting | SFBAudioMetadataKindGrouping | SFBAudioMetadataKindAdditional | SFBAudioMetadataKindReplayGain) from:metadata];
}

- (void) removeMetadataOfKind:(SFBAudioMetadataKind)kind
{
	if(kind & SFBAudioMetadataKindBasic) {
		self.title = nil;
		self.albumTitle = nil;
		self.artist = nil;
		self.albumArtist = nil;
		self.genre = nil;
		self.composer = nil;
		self.releaseDate = nil;
		self.compilation = nil;
		self.trackNumber = nil;
		self.trackTotal = nil;
		self.discNumber = nil;
		self.discTotal = nil;
		self.lyrics = nil;
		self.bpm = nil;
		self.rating = nil;
		self.comment = nil;
		self.isrc = nil;
		self.mcn = nil;
		self.musicBrainzReleaseID = nil;
		self.musicBrainzRecordingID = nil;
	}

	if(kind & SFBAudioMetadataKindSorting) {
		self.titleSortOrder = nil;
		self.albumTitleSortOrder = nil;
		self.artistSortOrder = nil;
		self.albumArtistSortOrder = nil;
		self.composerSortOrder = nil;
		self.genreSortOrder = nil;
	}

	if(kind & SFBAudioMetadataKindGrouping)
		self.grouping = nil;

	if(kind & SFBAudioMetadataKindAdditional)
		self.additionalMetadata = nil;

	if(kind & SFBAudioMetadataKindReplayGain) {
		self.replayGainReferenceLoudness = nil;
		self.replayGainTrackGain = nil;
		self.replayGainTrackPeak = nil;
		self.replayGainAlbumGain = nil;
		self.replayGainAlbumPeak = nil;
	}
}

- (void) removeAllMetadata
{
	[self removeMetadataOfKind:(SFBAudioMetadataKindBasic | SFBAudioMetadataKindSorting | SFBAudioMetadataKindGrouping | SFBAudioMetadataKindAdditional | SFBAudioMetadataKindReplayGain)];
}

#pragma mark Attached Pictures

- (NSSet *)attachedPictures
{
	return [_pictures copy];
}

#pragma mark Attached Picture Utilities

- (void)copyAttachedPicturesFrom:(SFBAudioMetadata *)metadata
{
	for(SFBAttachedPicture *picture in metadata.attachedPictures)
		[_pictures addObject:picture];
}

- (NSArray *)attachedPicturesOfType:(SFBAttachedPictureType)type
{
	NSMutableArray *pictures = [NSMutableArray array];
	for(SFBAttachedPicture *picture in _pictures) {
		if(picture.pictureType == type)
			[pictures addObject:picture];
	}
	return pictures;
}

- (void)attachPicture:(SFBAttachedPicture *)picture
{
	[_pictures addObject:picture];
}

- (void)removeAttachedPicture:(SFBAttachedPicture *)picture
{
	[_pictures removeObject:picture];
}

- (void)removeAttachedPicturesOfType:(SFBAttachedPictureType)type
{
	for(SFBAttachedPicture *picture in _pictures) {
		if(picture.pictureType == type)
			[_pictures removeObject:picture];
	}
}

- (void)removeAllAttachedPictures
{
	[_pictures removeAllObjects];
}

#pragma mark External Representation

- (NSDictionary *)dictionaryRepresentation
{
	NSMutableDictionary *dictionary = [_metadata mutableCopy];
	NSMutableArray *pictures = [NSMutableArray arrayWithCapacity:_pictures.count];
	for(SFBAttachedPicture *picture in _pictures)
		[pictures addObject:picture.dictionaryRepresentation];
	dictionary[SFBAudioMetadataKeyAttachedPictures] = pictures;
	return dictionary;
}

- (void)setFromDictionaryRepresentation:(NSDictionary *)dictionary
{
	self.title = dictionary[SFBAudioMetadataKeyTitle];
	self.albumTitle = dictionary[SFBAudioMetadataKeyAlbumTitle];
	self.artist = dictionary[SFBAudioMetadataKeyArtist];
	self.albumArtist = dictionary[SFBAudioMetadataKeyAlbumArtist];
	self.genre = dictionary[SFBAudioMetadataKeyGenre];
	self.composer = dictionary[SFBAudioMetadataKeyComposer];
	self.releaseDate = dictionary[SFBAudioMetadataKeyReleaseDate];
	self.compilation = dictionary[SFBAudioMetadataKeyCompilation];
	self.trackNumber = dictionary[SFBAudioMetadataKeyTrackNumber];
	self.trackTotal = dictionary[SFBAudioMetadataKeyTrackTotal];
	self.discNumber = dictionary[SFBAudioMetadataKeyDiscNumber];
	self.discTotal = dictionary[SFBAudioMetadataKeyDiscTotal];
	self.lyrics = dictionary[SFBAudioMetadataKeyLyrics];
	self.bpm = dictionary[SFBAudioMetadataKeyBPM];
	self.rating = dictionary[SFBAudioMetadataKeyRating];
	self.comment = dictionary[SFBAudioMetadataKeyComment];
	self.isrc = dictionary[SFBAudioMetadataKeyISRC];
	self.mcn = dictionary[SFBAudioMetadataKeyMCN];
	self.musicBrainzReleaseID = dictionary[SFBAudioMetadataKeyMusicBrainzReleaseID];
	self.musicBrainzRecordingID = dictionary[SFBAudioMetadataKeyMusicBrainzRecordingID];

	self.titleSortOrder = dictionary[SFBAudioMetadataKeyTitleSortOrder];
	self.albumTitleSortOrder = dictionary[SFBAudioMetadataKeyAlbumTitleSortOrder];
	self.artistSortOrder = dictionary[SFBAudioMetadataKeyArtistSortOrder];
	self.albumArtistSortOrder = dictionary[SFBAudioMetadataKeyAlbumArtistSortOrder];
	self.composerSortOrder = dictionary[SFBAudioMetadataKeyComposerSortOrder];
	self.genreSortOrder = dictionary[SFBAudioMetadataKeyGenreSortOrder];

	self.grouping = dictionary[SFBAudioMetadataKeyGrouping];

	self.additionalMetadata = dictionary[SFBAudioMetadataKeyAdditionalMetadata];

	self.replayGainReferenceLoudness = dictionary[SFBAudioMetadataKeyReplayGainReferenceLoudness];
	self.replayGainTrackGain = dictionary[SFBAudioMetadataKeyReplayGainTrackGain];
	self.replayGainTrackPeak = dictionary[SFBAudioMetadataKeyReplayGainTrackPeak];
	self.replayGainAlbumGain = dictionary[SFBAudioMetadataKeyReplayGainAlbumGain];
	self.replayGainAlbumPeak = dictionary[SFBAudioMetadataKeyReplayGainAlbumPeak];

	NSArray *pictures = dictionary[SFBAudioMetadataKeyAttachedPictures];
	for(NSDictionary *picture in pictures)
		[self attachPicture:[[SFBAttachedPicture alloc] initWithDictionaryRepresentation:picture]];
}

@end
