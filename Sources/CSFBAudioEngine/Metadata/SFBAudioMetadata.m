//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBAudioMetadata.h"

// Key names for the metadata dictionary
SFBAudioMetadataKey const SFBAudioMetadataKeyTitle = @"Title";
SFBAudioMetadataKey const SFBAudioMetadataKeyArtist = @"Artist";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitle = @"Album Title";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtist = @"Album Artist";
SFBAudioMetadataKey const SFBAudioMetadataKeyComposer = @"Composer";
SFBAudioMetadataKey const SFBAudioMetadataKeyGenre = @"Genre";
SFBAudioMetadataKey const SFBAudioMetadataKeyReleaseDate = @"Date";
SFBAudioMetadataKey const SFBAudioMetadataKeyCompilation = @"Compilation";
SFBAudioMetadataKey const SFBAudioMetadataKeyTrackNumber = @"Track Number";
SFBAudioMetadataKey const SFBAudioMetadataKeyTrackTotal = @"Track Total";
SFBAudioMetadataKey const SFBAudioMetadataKeyDiscNumber = @"Disc Number";
SFBAudioMetadataKey const SFBAudioMetadataKeyDiscTotal = @"Disc Total";
SFBAudioMetadataKey const SFBAudioMetadataKeyLyrics = @"Lyrics";
SFBAudioMetadataKey const SFBAudioMetadataKeyBPM = @"BPM";
SFBAudioMetadataKey const SFBAudioMetadataKeyRating = @"Rating";
SFBAudioMetadataKey const SFBAudioMetadataKeyComment = @"Comment";
SFBAudioMetadataKey const SFBAudioMetadataKeyISRC = @"ISRC";
SFBAudioMetadataKey const SFBAudioMetadataKeyMCN = @"MCN";
SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzReleaseID = @"MusicBrainz Release ID";
SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzRecordingID = @"MusicBrainz Recording ID";

SFBAudioMetadataKey const SFBAudioMetadataKeyTitleSortOrder = @"Title Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyArtistSortOrder = @"Artist Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitleSortOrder = @"Album Title Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtistSortOrder = @"Album Artist Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyComposerSortOrder = @"Composer Sort Order";
SFBAudioMetadataKey const SFBAudioMetadataKeyGenreSortOrder = @"Genre Sort Order";

SFBAudioMetadataKey const SFBAudioMetadataKeyGrouping = @"Grouping";

SFBAudioMetadataKey const SFBAudioMetadataKeyAdditionalMetadata = @"Additional Metadata";

SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainReferenceLoudness = @"Replay Gain Reference Loudness";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackGain = @"Replay Gain Track Gain";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackPeak = @"Replay Gain Track Peak";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumGain = @"Replay Gain Album Gain";
SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumPeak = @"Replay Gain Album Peak";

SFBAudioMetadataKey const SFBAudioMetadataKeyAttachedPictures = @"Attached Pictures";

@interface SFBAudioMetadata () {
  @private
    NSMutableDictionary *_metadata;
    NSMutableSet *_pictures;
}
+ (id)sharedKeySet;
@end

@implementation SFBAudioMetadata

static id _sharedKeySet;

+ (id)sharedKeySet {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _sharedKeySet = [NSDictionary sharedKeySetForKeys:@[
            SFBAudioMetadataKeyTitle,
            SFBAudioMetadataKeyArtist,
            SFBAudioMetadataKeyAlbumTitle,
            SFBAudioMetadataKeyAlbumArtist,
            SFBAudioMetadataKeyComposer,
            SFBAudioMetadataKeyGenre,
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

- (instancetype)init {
    if ((self = [super init])) {
        _metadata = [NSMutableDictionary dictionaryWithSharedKeySet:[SFBAudioMetadata sharedKeySet]];
        _pictures = [NSMutableSet set];
    }
    return self;
}

- (instancetype)initWithDictionaryRepresentation:(NSDictionary *)dictionaryRepresentation {
    if ((self = [self init])) {
        [self setFromDictionaryRepresentation:dictionaryRepresentation];
    }
    return self;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone {
#pragma unused(zone)
    SFBAudioMetadata *result = [[[self class] alloc] init];
    result->_metadata = [_metadata mutableCopy];
    result->_pictures = [_pictures mutableCopy];
    return result;
}

- (void)removeAll {
    [_metadata removeAllObjects];
    [_pictures removeAllObjects];
}

// MARK: - Basic Metadata

- (NSString *)title {
    return [_metadata objectForKey:SFBAudioMetadataKeyTitle];
}

- (void)setTitle:(NSString *)title {
    [_metadata setValue:title forKey:SFBAudioMetadataKeyTitle];
}

- (NSString *)artist {
    return [_metadata objectForKey:SFBAudioMetadataKeyArtist];
}

- (void)setArtist:(NSString *)artist {
    [_metadata setValue:artist forKey:SFBAudioMetadataKeyArtist];
}

- (NSString *)albumTitle {
    return [_metadata objectForKey:SFBAudioMetadataKeyAlbumTitle];
}

- (void)setAlbumTitle:(NSString *)albumTitle {
    [_metadata setValue:albumTitle forKey:SFBAudioMetadataKeyAlbumTitle];
}

- (NSString *)albumArtist {
    return [_metadata objectForKey:SFBAudioMetadataKeyAlbumArtist];
}

- (void)setAlbumArtist:(NSString *)albumArtist {
    [_metadata setValue:albumArtist forKey:SFBAudioMetadataKeyAlbumArtist];
}

- (NSString *)composer {
    return [_metadata objectForKey:SFBAudioMetadataKeyComposer];
}

- (void)setComposer:(NSString *)composer {
    [_metadata setValue:composer forKey:SFBAudioMetadataKeyComposer];
}

- (NSString *)genre {
    return [_metadata objectForKey:SFBAudioMetadataKeyGenre];
}

- (void)setGenre:(NSString *)genre {
    [_metadata setValue:genre forKey:SFBAudioMetadataKeyGenre];
}

- (NSString *)releaseDate {
    return [_metadata objectForKey:SFBAudioMetadataKeyReleaseDate];
}

- (void)setReleaseDate:(NSString *)releaseDate {
    [_metadata setValue:releaseDate forKey:SFBAudioMetadataKeyReleaseDate];
}

- (NSNumber *)compilation {
    return [_metadata objectForKey:SFBAudioMetadataKeyCompilation];
}

- (void)setCompilation:(NSNumber *)compilation {
    [_metadata setValue:compilation forKey:SFBAudioMetadataKeyCompilation];
}

- (NSNumber *)trackNumber {
    return [_metadata objectForKey:SFBAudioMetadataKeyTrackNumber];
}

- (void)setTrackNumber:(NSNumber *)trackNumber {
    [_metadata setValue:trackNumber forKey:SFBAudioMetadataKeyTrackNumber];
}

- (NSNumber *)trackTotal {
    return [_metadata objectForKey:SFBAudioMetadataKeyTrackTotal];
}

- (void)setTrackTotal:(NSNumber *)trackTotal {
    [_metadata setValue:trackTotal forKey:SFBAudioMetadataKeyTrackTotal];
}

- (NSNumber *)discNumber {
    return [_metadata objectForKey:SFBAudioMetadataKeyDiscNumber];
}

- (void)setDiscNumber:(NSNumber *)discNumber {
    [_metadata setValue:discNumber forKey:SFBAudioMetadataKeyDiscNumber];
}

- (NSNumber *)discTotal {
    return [_metadata objectForKey:SFBAudioMetadataKeyDiscTotal];
}

- (void)setDiscTotal:(NSNumber *)discTotal {
    [_metadata setValue:discTotal forKey:SFBAudioMetadataKeyDiscTotal];
}

- (NSString *)lyrics {
    return [_metadata objectForKey:SFBAudioMetadataKeyLyrics];
}

- (void)setLyrics:(NSString *)lyrics {
    [_metadata setValue:lyrics forKey:SFBAudioMetadataKeyLyrics];
}

- (NSNumber *)bpm {
    return [_metadata objectForKey:SFBAudioMetadataKeyBPM];
}

- (void)setBpm:(NSNumber *)bpm {
    [_metadata setValue:bpm forKey:SFBAudioMetadataKeyBPM];
}

- (NSNumber *)rating {
    return [_metadata objectForKey:SFBAudioMetadataKeyRating];
}

- (void)setRating:(NSNumber *)rating {
    [_metadata setValue:rating forKey:SFBAudioMetadataKeyRating];
}

- (NSString *)comment {
    return [_metadata objectForKey:SFBAudioMetadataKeyComment];
}

- (void)setComment:(NSString *)comment {
    [_metadata setValue:comment forKey:SFBAudioMetadataKeyComment];
}

- (NSString *)isrc {
    return [_metadata objectForKey:SFBAudioMetadataKeyISRC];
}

- (void)setIsrc:(NSString *)isrc {
    [_metadata setValue:isrc forKey:SFBAudioMetadataKeyISRC];
}

- (NSString *)mcn {
    return [_metadata objectForKey:SFBAudioMetadataKeyMCN];
}

- (void)setMcn:(NSString *)mcn {
    [_metadata setValue:mcn forKey:SFBAudioMetadataKeyMCN];
}

- (NSString *)musicBrainzReleaseID {
    return [_metadata objectForKey:SFBAudioMetadataKeyMusicBrainzReleaseID];
}

- (void)setMusicBrainzReleaseID:(NSString *)musicBrainzReleaseID {
    [_metadata setValue:musicBrainzReleaseID forKey:SFBAudioMetadataKeyMusicBrainzReleaseID];
}

- (NSString *)musicBrainzRecordingID {
    return [_metadata objectForKey:SFBAudioMetadataKeyMusicBrainzRecordingID];
}

- (void)setMusicBrainzRecordingID:(NSString *)musicBrainzRecordingID {
    [_metadata setValue:musicBrainzRecordingID forKey:SFBAudioMetadataKeyMusicBrainzRecordingID];
}

// MARK: - Sorting Metadata

- (NSString *)titleSortOrder {
    return [_metadata objectForKey:SFBAudioMetadataKeyTitleSortOrder];
}

- (void)setTitleSortOrder:(NSString *)titleSortOrder {
    [_metadata setValue:titleSortOrder forKey:SFBAudioMetadataKeyTitleSortOrder];
}

- (NSString *)artistSortOrder {
    return [_metadata objectForKey:SFBAudioMetadataKeyArtistSortOrder];
}

- (void)setArtistSortOrder:(NSString *)artistSortOrder {
    [_metadata setValue:artistSortOrder forKey:SFBAudioMetadataKeyArtistSortOrder];
}

- (NSString *)albumTitleSortOrder {
    return [_metadata objectForKey:SFBAudioMetadataKeyAlbumTitleSortOrder];
}

- (void)setAlbumTitleSortOrder:(NSString *)albumTitleSortOrder {
    [_metadata setValue:albumTitleSortOrder forKey:SFBAudioMetadataKeyAlbumTitleSortOrder];
}

- (NSString *)albumArtistSortOrder {
    return [_metadata objectForKey:SFBAudioMetadataKeyAlbumArtistSortOrder];
}

- (void)setAlbumArtistSortOrder:(NSString *)albumArtistSortOrder {
    [_metadata setValue:albumArtistSortOrder forKey:SFBAudioMetadataKeyAlbumArtistSortOrder];
}

- (NSString *)composerSortOrder {
    return [_metadata objectForKey:SFBAudioMetadataKeyComposerSortOrder];
}

- (void)setComposerSortOrder:(NSString *)composerSortOrder {
    [_metadata setValue:composerSortOrder forKey:SFBAudioMetadataKeyComposerSortOrder];
}

- (NSString *)genreSortOrder {
    return [_metadata objectForKey:SFBAudioMetadataKeyGenreSortOrder];
}

- (void)setGenreSortOrder:(NSString *)genreSortOrder {
    [_metadata setValue:genreSortOrder forKey:SFBAudioMetadataKeyGenreSortOrder];
}

// MARK: - Grouping Metadata

- (NSString *)grouping {
    return [_metadata objectForKey:SFBAudioMetadataKeyGrouping];
}

- (void)setGrouping:(NSString *)grouping {
    [_metadata setValue:grouping forKey:SFBAudioMetadataKeyGrouping];
}

// MARK: - Additional Metadata

- (NSDictionary *)additionalMetadata {
    return [_metadata objectForKey:SFBAudioMetadataKeyAdditionalMetadata];
}

- (void)setAdditionalMetadata:(NSDictionary *)additionalMetadata {
    [_metadata setValue:[additionalMetadata copy] forKey:SFBAudioMetadataKeyAdditionalMetadata];
}

// MARK: - Replay Gain Metadata

- (NSNumber *)replayGainReferenceLoudness {
    return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainReferenceLoudness];
}

- (void)setReplayGainReferenceLoudness:(NSNumber *)replayGainReferenceLoudness {
    [_metadata setValue:replayGainReferenceLoudness forKey:SFBAudioMetadataKeyReplayGainReferenceLoudness];
}

- (NSNumber *)replayGainTrackGain {
    return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainTrackGain];
}

- (void)setReplayGainTrackGain:(NSNumber *)replayGainTrackGain {
    [_metadata setValue:replayGainTrackGain forKey:SFBAudioMetadataKeyReplayGainTrackGain];
}

- (NSNumber *)replayGainTrackPeak {
    return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainTrackPeak];
}

- (void)setReplayGainTrackPeak:(NSNumber *)replayGainTrackPeak {
    [_metadata setValue:replayGainTrackPeak forKey:SFBAudioMetadataKeyReplayGainTrackPeak];
}

- (NSNumber *)replayGainAlbumGain {
    return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainAlbumGain];
}

- (void)setReplayGainAlbumGain:(NSNumber *)replayGainAlbumGain {
    [_metadata setValue:replayGainAlbumGain forKey:SFBAudioMetadataKeyReplayGainAlbumGain];
}

- (NSNumber *)replayGainAlbumPeak {
    return [_metadata objectForKey:SFBAudioMetadataKeyReplayGainAlbumPeak];
}

- (void)setReplayGainAlbumPeak:(NSNumber *)replayGainAlbumPeak {
    [_metadata setValue:replayGainAlbumPeak forKey:SFBAudioMetadataKeyReplayGainAlbumPeak];
}

// MARK: - Metadata Utilities

- (void)copyMetadataOfKind:(SFBAudioMetadataKind)kind from:(SFBAudioMetadata *)metadata {
    if (kind & SFBAudioMetadataKindBasic) {
        self.title = metadata.title;
        self.artist = metadata.artist;
        self.albumTitle = metadata.albumTitle;
        self.albumArtist = metadata.albumArtist;
        self.composer = metadata.composer;
        self.genre = metadata.genre;
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

    if (kind & SFBAudioMetadataKindSorting) {
        self.titleSortOrder = metadata.titleSortOrder;
        self.artistSortOrder = metadata.artistSortOrder;
        self.albumTitleSortOrder = metadata.albumTitleSortOrder;
        self.albumArtistSortOrder = metadata.albumArtistSortOrder;
        self.composerSortOrder = metadata.composerSortOrder;
        self.genreSortOrder = metadata.genreSortOrder;
    }

    if (kind & SFBAudioMetadataKindGrouping) {
        self.grouping = metadata.grouping;
    }

    if (kind & SFBAudioMetadataKindAdditional) {
        self.additionalMetadata = metadata.additionalMetadata;
    }

    if (kind & SFBAudioMetadataKindReplayGain) {
        self.replayGainReferenceLoudness = metadata.replayGainReferenceLoudness;
        self.replayGainTrackGain = metadata.replayGainTrackGain;
        self.replayGainTrackPeak = metadata.replayGainTrackPeak;
        self.replayGainAlbumGain = metadata.replayGainAlbumGain;
        self.replayGainAlbumPeak = metadata.replayGainAlbumPeak;
    }
}

- (void)copyMetadataFrom:(SFBAudioMetadata *)metadata {
    [self copyMetadataOfKind:(SFBAudioMetadataKindBasic | SFBAudioMetadataKindSorting | SFBAudioMetadataKindGrouping |
                              SFBAudioMetadataKindAdditional | SFBAudioMetadataKindReplayGain)
                        from:metadata];
}

- (void)removeMetadataOfKind:(SFBAudioMetadataKind)kind {
    if (kind & SFBAudioMetadataKindBasic) {
        self.title = nil;
        self.artist = nil;
        self.albumTitle = nil;
        self.albumArtist = nil;
        self.composer = nil;
        self.genre = nil;
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

    if (kind & SFBAudioMetadataKindSorting) {
        self.titleSortOrder = nil;
        self.artistSortOrder = nil;
        self.albumTitleSortOrder = nil;
        self.albumArtistSortOrder = nil;
        self.composerSortOrder = nil;
        self.genreSortOrder = nil;
    }

    if (kind & SFBAudioMetadataKindGrouping) {
        self.grouping = nil;
    }

    if (kind & SFBAudioMetadataKindAdditional) {
        self.additionalMetadata = nil;
    }

    if (kind & SFBAudioMetadataKindReplayGain) {
        self.replayGainReferenceLoudness = nil;
        self.replayGainTrackGain = nil;
        self.replayGainTrackPeak = nil;
        self.replayGainAlbumGain = nil;
        self.replayGainAlbumPeak = nil;
    }
}

- (void)removeAllMetadata {
    [_metadata removeAllObjects];
}

// MARK: - Attached Pictures

- (NSSet *)attachedPictures {
    return [_pictures copy];
}

// MARK: - Attached Picture Utilities

- (void)copyAttachedPicturesFrom:(SFBAudioMetadata *)metadata {
    for (SFBAttachedPicture *picture in metadata.attachedPictures) {
        [_pictures addObject:picture];
    }
}

- (NSArray *)attachedPicturesOfType:(SFBAttachedPictureType)type {
    NSMutableArray *pictures = [NSMutableArray array];
    for (SFBAttachedPicture *picture in _pictures) {
        if (picture.pictureType == type) {
            [pictures addObject:picture];
        }
    }
    return pictures;
}

- (void)attachPicture:(SFBAttachedPicture *)picture {
    [_pictures addObject:picture];
}

- (void)removeAttachedPicture:(SFBAttachedPicture *)picture {
    [_pictures removeObject:picture];
}

- (void)removeAttachedPicturesOfType:(SFBAttachedPictureType)type {
    NSSet *picturesToRemove = [_pictures objectsPassingTest:^BOOL(SFBAttachedPicture *obj, BOOL *stop) {
#pragma unused(stop)
        return obj.pictureType == type;
    }];
    [_pictures minusSet:picturesToRemove];
}

- (void)removeAllAttachedPictures {
    [_pictures removeAllObjects];
}

// MARK: - External Representation

- (NSDictionary *)dictionaryRepresentation {
    NSMutableDictionary *dictionary = [_metadata mutableCopy];
    NSMutableArray *pictures = [NSMutableArray arrayWithCapacity:_pictures.count];
    for (SFBAttachedPicture *picture in _pictures) {
        [pictures addObject:picture.dictionaryRepresentation];
    }
    dictionary[SFBAudioMetadataKeyAttachedPictures] = pictures;
    return dictionary;
}

- (void)setFromDictionaryRepresentation:(NSDictionary *)dictionary {
    self.title = dictionary[SFBAudioMetadataKeyTitle];
    self.artist = dictionary[SFBAudioMetadataKeyArtist];
    self.albumTitle = dictionary[SFBAudioMetadataKeyAlbumTitle];
    self.albumArtist = dictionary[SFBAudioMetadataKeyAlbumArtist];
    self.composer = dictionary[SFBAudioMetadataKeyComposer];
    self.genre = dictionary[SFBAudioMetadataKeyGenre];
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
    self.artistSortOrder = dictionary[SFBAudioMetadataKeyArtistSortOrder];
    self.albumTitleSortOrder = dictionary[SFBAudioMetadataKeyAlbumTitleSortOrder];
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
    for (NSDictionary *picture in pictures) {
        [self attachPicture:[[SFBAttachedPicture alloc] initWithDictionaryRepresentation:picture]];
    }
}

// MARK: - Dictionary-Like Interface

- (id)objectForKey:(SFBAudioMetadataKey)key {
    return [_metadata objectForKey:key];
}

- (void)setObject:(id)obj forKey:(SFBAudioMetadataKey)key {
    [_metadata setObject:obj forKey:key];
}

- (void)removeObjectForKey:(SFBAudioMetadataKey)key {
    [_metadata removeObjectForKey:key];
}

- (id)valueForKey:(SFBAudioMetadataKey)key {
    return [_metadata valueForKey:key];
}

- (void)setValue:(nullable id)obj forKey:(SFBAudioMetadataKey)key {
    [_metadata setValue:obj forKey:key];
}

- (id)objectForKeyedSubscript:(SFBAudioMetadataKey)key {
    return _metadata[key];
}

- (void)setObject:(nullable id)obj forKeyedSubscript:(SFBAudioMetadataKey)key {
    _metadata[key] = obj;
}

@end
