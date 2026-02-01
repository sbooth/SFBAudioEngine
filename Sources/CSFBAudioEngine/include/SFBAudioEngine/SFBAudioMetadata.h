//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBAttachedPicture.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Metadata kind bitmask values used in `copyMetadataOfKind:from:` and `removeMetadataOfKind:`
typedef NS_OPTIONS(NSUInteger, SFBAudioMetadataKind) {
    /// Basic metadata
    SFBAudioMetadataKindBasic = 1 << 0,
    /// Sorting metadata
    SFBAudioMetadataKindSorting = 1 << 1,
    /// Grouping metadata
    SFBAudioMetadataKindGrouping = 1 << 2,
    /// Additional metadata
    SFBAudioMetadataKindAdditional = 1 << 3,
    /// Replay gain metadata
    SFBAudioMetadataKindReplayGain = 1 << 4,
} NS_SWIFT_NAME(AudioMetadata.Kind);

/// A key in an audio metadata dictionary
typedef NSString *SFBAudioMetadataKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioMetadata.Key);

// Basic metadata dictionary keys
/// Title (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTitle;
/// Artist (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyArtist;
/// Album title (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitle;
/// Album artist (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtist;
/// Composer (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyComposer;
/// Genre (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyGenre;
/// Release date (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReleaseDate;
/// Compilation flag (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyCompilation;
/// Track number (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTrackNumber;
/// Track total (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTrackTotal;
/// Disc number (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyDiscNumber;
/// Disc total (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyDiscTotal;
/// Lyrics (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyLyrics;
/// Beats per minute (BPM) (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyBPM;
/// Rating (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyRating;
/// Comment (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyComment;
/// International Standard Recording Code (ISRC) (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyISRC;
/// Media Catalog Number (MCN) (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyMCN;
/// MusicBrainz release ID (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzReleaseID;
/// MusicBrainz recording ID (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzRecordingID;

// Sorting dictionary keys
/// Title sort order (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTitleSortOrder;
/// Artist sort order (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyArtistSortOrder;
/// Album title sort order (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitleSortOrder;
/// Album artist sort order (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtistSortOrder;
/// Composer sort order (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyComposerSortOrder;
/// Genre sort order (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyGenreSortOrder;

// Grouping dictionary keys
/// Grouping (`NSString`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyGrouping;

// Additional metadata dictionary keys
/// Additional metadata (`NSDictionary`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAdditionalMetadata;

// Replay gain dictionary keys
/// Replay gain reference loudness (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainReferenceLoudness;
/// Replay gain track gain (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackGain;
/// Replay gain track peak (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackPeak;
/// Replay gain album gain (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumGain;
/// Replay gain album peak (`NSNumber`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumPeak;

// Attached Picture dictionary keys
/// Attached pictures (`NSArray` of `NSDictionary`)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAttachedPictures;

/// Class supporting commonly-used audio metadata and attached pictures
NS_SWIFT_NAME(AudioMetadata)
@interface SFBAudioMetadata : NSObject <NSCopying>

/// Returns an initialized empty `SFBAudioMetadata` object
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/// Returns an initialized `SFBAudioMetadata` object populated with values from `dictionaryRepresentation`
/// - parameter dictionaryRepresentation: A dictionary containing the desired values
- (instancetype)initWithDictionaryRepresentation:(NSDictionary<SFBAudioMetadataKey, id> *)dictionaryRepresentation;

/// Removes all metadata and attached pictures
/// - seealso: `-removeAllMetadata`
/// - seealso: `-removeAllAttachedPictures`
- (void)removeAll;

// MARK: - Basic Metadata

/// The title
@property(nonatomic, nullable) NSString *title;

/// The artist
@property(nonatomic, nullable) NSString *artist;

/// The album title
@property(nonatomic, nullable) NSString *albumTitle;

/// The album artist
@property(nonatomic, nullable) NSString *albumArtist;

/// The composer
@property(nonatomic, nullable) NSString *composer;

/// The genre
@property(nonatomic, nullable) NSString *genre;

/// The release date
@property(nonatomic, nullable) NSString *releaseDate;

/// The compilation flag
@property(nonatomic, nullable) NSNumber *compilation NS_REFINED_FOR_SWIFT;

/// The track number
@property(nonatomic, nullable) NSNumber *trackNumber NS_REFINED_FOR_SWIFT;

/// The track total
@property(nonatomic, nullable) NSNumber *trackTotal NS_REFINED_FOR_SWIFT;

/// The disc number
@property(nonatomic, nullable) NSNumber *discNumber NS_REFINED_FOR_SWIFT;

/// The disc total
@property(nonatomic, nullable) NSNumber *discTotal NS_REFINED_FOR_SWIFT;

/// The lyrics
@property(nonatomic, nullable) NSString *lyrics;

/// The Beats per minute (BPM)
@property(nonatomic, nullable) NSNumber *bpm NS_REFINED_FOR_SWIFT;

/// The rating
@property(nonatomic, nullable) NSNumber *rating NS_REFINED_FOR_SWIFT;

/// The comment
@property(nonatomic, nullable) NSString *comment;

/// The Media Catalog Number (MCN)
@property(nonatomic, nullable) NSString *mcn;

/// The International Standard Recording Code (ISRC)
@property(nonatomic, nullable) NSString *isrc;

/// The MusicBrainz release ID
@property(nonatomic, nullable) NSString *musicBrainzReleaseID;

/// The MusicBrainz recording ID
@property(nonatomic, nullable) NSString *musicBrainzRecordingID;

// MARK: - Sorting Metadata

/// The title sort order
@property(nonatomic, nullable) NSString *titleSortOrder;

/// The artist sort order
@property(nonatomic, nullable) NSString *artistSortOrder;

/// The album title sort order
@property(nonatomic, nullable) NSString *albumTitleSortOrder;

/// The album artist sort order
@property(nonatomic, nullable) NSString *albumArtistSortOrder;

/// The composer sort order
@property(nonatomic, nullable) NSString *composerSortOrder;

/// The genre sort order
@property(nonatomic, nullable) NSString *genreSortOrder;

// MARK: - Grouping Metadata

/// The grouping
@property(nonatomic, nullable) NSString *grouping;

// MARK: - Additional Metadata

/// The additional metadata
@property(nonatomic, nullable) NSDictionary *additionalMetadata;

// MARK: - ReplayGain Metadata

/// The replay gain reference loudness
@property(nonatomic, nullable) NSNumber *replayGainReferenceLoudness NS_REFINED_FOR_SWIFT;

/// The replay gain track gain
@property(nonatomic, nullable) NSNumber *replayGainTrackGain NS_REFINED_FOR_SWIFT;

/// The replay gain track peak
@property(nonatomic, nullable) NSNumber *replayGainTrackPeak NS_REFINED_FOR_SWIFT;

/// The replay gain album gain
@property(nonatomic, nullable) NSNumber *replayGainAlbumGain NS_REFINED_FOR_SWIFT;

/// The replay gain album peak
@property(nonatomic, nullable) NSNumber *replayGainAlbumPeak NS_REFINED_FOR_SWIFT;

// MARK: - Metadata Utilities

/// Copies all metadata from `metadata`
/// - note: Does not copy album artwork
/// - parameter metadata: An `SFBAudioMetadata` object containing the metadata to copy
/// - seealso: `-copyMetadataOfKind:from:`
/// - seealso: `-copyAttachedPictures:`
- (void)copyMetadataFrom:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyMetadata(from:));

/// Copies the values contained in the specified metadata kinds from `metadata`
/// - note: Does not copy album artwork
/// - parameter metadata: An `SFBAudioMetadata` object containing the metadata to copy
/// - parameter kind: A bitmask specifying the kinds of metadata to copy
/// - seealso: `-copyMetadataFrom:`
/// - seealso: `-copyAttachedPictures`
- (void)copyMetadataOfKind:(SFBAudioMetadataKind)kind
                      from:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyMetadata(ofKind:from:));

/// Sets the values contained in the specified metadata kinds to `nil`
/// - parameter kind: A bitmask specifying the kinds of metadata to remove
/// - seealso: `-removeAllMetadata`
/// - seealso: `-removeAllAttachedPictures`
- (void)removeMetadataOfKind:(SFBAudioMetadataKind)kind NS_SWIFT_NAME(removeMetadata(ofKind:));

/// Sets all metadata to `nil`
/// - note: Leaves album art intact
/// - seealso: `-removeMetadataOfKind:`
/// - seealso: `-removeAllAttachedPictures`
- (void)removeAllMetadata;

// MARK: - Attached Pictures

/// Get all attached pictures
@property(nonatomic, readonly) NSSet<SFBAttachedPicture *> *attachedPictures;

// MARK: - Attached Picture Utilities

/// Copies album artwork from `metadata`
/// - note: This clears existing album artwork
/// - note: Does not copy metadata
/// - parameter metadata: An `SFBAudioMetadata` object containing the artwork to copy
/// - seealso: `-copyMetadataFrom:`
- (void)copyAttachedPicturesFrom:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyAttachedPicturesFrom(_:));

/// Get all attached pictures of the specified type
- (NSArray<SFBAttachedPicture *> *)attachedPicturesOfType:(SFBAttachedPictureType)type
        NS_SWIFT_NAME(attachedPictures(ofType:));

/// Attach a picture
- (void)attachPicture:(SFBAttachedPicture *)picture NS_SWIFT_NAME(attachPicture(_:));

/// Remove an attached picture
- (void)removeAttachedPicture:(SFBAttachedPicture *)picture NS_SWIFT_NAME(removeAttachedPicture(_:));

/// Remove all attached pictures of the specified type
- (void)removeAttachedPicturesOfType:(SFBAttachedPictureType)type NS_SWIFT_NAME(removeAttachedPicturesOfType(_:));

/// Remove all attached pictures
- (void)removeAllAttachedPictures;

// MARK: - External Representation

/// Copy the values contained in this object to a dictionary
/// - returns: A dictionary containing this object's metadata and attached pictures
@property(nonatomic, readonly) NSDictionary<SFBAudioMetadataKey, id> *dictionaryRepresentation;

/// Sets the metadata and attached pictures contained in this object from a dictionary
/// - parameter dictionary: A dictionary containing the desired values
- (void)setFromDictionaryRepresentation:(NSDictionary<SFBAudioMetadataKey, id> *)dictionary NS_SWIFT_NAME(setFrom(_:));

// MARK: - Dictionary-Like Interface

/// Returns the metadata value for a key
/// - parameter key: The key for the desired metadata value
/// - returns: The metadata value for `key`
- (nullable id)objectForKey:(SFBAudioMetadataKey)key;
/// Sets the metadata value for a key
/// - parameter obj: The metadata value to set
/// - parameter key: The key for the metadata value
- (void)setObject:(id)obj forKey:(SFBAudioMetadataKey)key;
/// Removes the metadata value for a key
/// - parameter key: The key for the metadata value to remove
- (void)removeObjectForKey:(SFBAudioMetadataKey)key;

/// Returns the metadata value for a key
/// - parameter key: The key for the desired metadata value
/// - returns: The metadata value for `key`
- (nullable id)valueForKey:(SFBAudioMetadataKey)key;
/// Sets or removes a metadata value
/// - parameter obj: The metadata value to set or `nil` to remove
/// - parameter key: The key for the metadata value
- (void)setValue:(nullable id)obj forKey:(SFBAudioMetadataKey)key;

/// Returns the metadata value for a key
/// - parameter key: The key for the desired metadata value
/// - returns: The metadata value for `key`
- (nullable id)objectForKeyedSubscript:(SFBAudioMetadataKey)key;
/// Sets or removes a metadata value
/// - parameter obj: The metadata value to set or `nil` to remove
/// - parameter key: The key for the metadata value
- (void)setObject:(nullable id)obj forKeyedSubscript:(SFBAudioMetadataKey)key;

@end

NS_ASSUME_NONNULL_END
