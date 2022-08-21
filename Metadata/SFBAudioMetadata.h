//
// Copyright (c) 2006 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBAttachedPicture.h>

NS_ASSUME_NONNULL_BEGIN

/// Metadata kind bitmask values used in copyMetadataOfKind:from: and removeMetadataOfKind:
typedef NS_OPTIONS(NSUInteger, SFBAudioMetadataKind) {
	/// Basic metadata
	SFBAudioMetadataKindBasic			= (1u << 0),
	/// Sorting metadata
	SFBAudioMetadataKindSorting			= (1u << 1),
	/// Grouping metadata
	SFBAudioMetadataKindGrouping		= (1u << 2),
	/// Additional metadata
	SFBAudioMetadataKindAdditional		= (1u << 3),
	/// Replay gain metadata
	SFBAudioMetadataKindReplayGain		= (1u << 4)
} NS_SWIFT_NAME(AudioMetadata.Kind);

/// A key in an audio metadata dictionary
typedef NSString * SFBAudioMetadataKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioMetadata.Key);

// Basic metadata dictionary keys
/// Title (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTitle;
/// Artist (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyArtist;
/// Album title (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitle;
/// Album artist (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtist;
/// Composer (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyComposer;
/// Genre (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyGenre;
/// Release date (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReleaseDate;
/// Compilation flag (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyCompilation;
/// Track number (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTrackNumber;
/// Track total (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTrackTotal;
/// Disc number (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyDiscNumber;
/// Disc total (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyDiscTotal;
/// Lyrics (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyLyrics;
/// Beats per minute (BPM) (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyBPM;
/// Rating (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyRating;
/// Comment (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyComment;
/// International Standard Recording Code (ISRC) (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyISRC;
/// Media Catalog Number (MCN) (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyMCN;
/// MusicBrainz release ID (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzReleaseID;
/// MusicBrainz recording ID (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyMusicBrainzRecordingID;

// Sorting dictionary keys
/// Title sort order (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyTitleSortOrder;
/// Artist sort order (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyArtistSortOrder;
/// Album title sort order (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumTitleSortOrder;
/// Album artist sort order (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAlbumArtistSortOrder;
/// Composer sort order (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyComposerSortOrder;
/// Genre sort order (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyGenreSortOrder;

// Grouping dictionary keys
/// Grouping (\c NSString)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyGrouping;

// Additional metadata dictionary keys
/// Additional metadata (\c NSDictionary)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAdditionalMetadata;

// Replay gain dictionary keys
/// Replay gain reference loudness (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainReferenceLoudness;
/// Replay gain track gain (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackGain;
/// Replay gain track peak (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainTrackPeak;
/// Replay gain album gain (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumGain;
/// Replay gain album peak (\c NSNumber)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyReplayGainAlbumPeak;

// Attached Picture dictionary keys
/// Attached pictures (\c NSArray of \c NSDictionary)
extern SFBAudioMetadataKey const SFBAudioMetadataKeyAttachedPictures;

/// Class supporting commonly-used audio metadata and attached pictures
NS_SWIFT_NAME(AudioMetadata) @interface SFBAudioMetadata : NSObject <NSCopying>

/// Returns an initialized empty \c SFBAudioMetadata object
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/// Returns an initialized \c SFBAudioMetadata object populated with values from \c dictionaryRepresentation
/// @param dictionaryRepresentation A dictionary containing the desired values
- (instancetype)initWithDictionaryRepresentation:(NSDictionary<SFBAudioMetadataKey, id> *)dictionaryRepresentation;

#pragma mark - Basic Metadata

/// The title
@property (nonatomic, nullable) NSString *title;

/// The artist
@property (nonatomic, nullable) NSString *artist;

/// The album title
@property (nonatomic, nullable) NSString *albumTitle;

/// The album artist
@property (nonatomic, nullable) NSString *albumArtist;

/// The composer
@property (nonatomic, nullable) NSString *composer;

/// The genre
@property (nonatomic, nullable) NSString *genre;

/// The release date
@property (nonatomic, nullable) NSString *releaseDate;

/// The compilation flag
@property (nonatomic, nullable) NSNumber *compilation NS_REFINED_FOR_SWIFT;

/// The track number
@property (nonatomic, nullable) NSNumber *trackNumber NS_REFINED_FOR_SWIFT;

/// The track total
@property (nonatomic, nullable) NSNumber *trackTotal NS_REFINED_FOR_SWIFT;

/// The disc number
@property (nonatomic, nullable) NSNumber *discNumber NS_REFINED_FOR_SWIFT;

/// The disc total
@property (nonatomic, nullable) NSNumber *discTotal NS_REFINED_FOR_SWIFT;

/// The lyrics
@property (nonatomic, nullable) NSString *lyrics;

/// The Beats per minute (BPM)
@property (nonatomic, nullable) NSNumber *bpm NS_REFINED_FOR_SWIFT;

/// The rating
@property (nonatomic, nullable) NSNumber *rating NS_REFINED_FOR_SWIFT;

/// The comment
@property (nonatomic, nullable) NSString *comment;

/// The Media Catalog Number (MCN)
@property (nonatomic, nullable) NSString *mcn;

/// The International Standard Recording Code (ISRC)
@property (nonatomic, nullable) NSString *isrc;

/// The MusicBrainz release ID
@property (nonatomic, nullable) NSString *musicBrainzReleaseID;

/// The MusicBrainz recording ID
@property (nonatomic, nullable) NSString *musicBrainzRecordingID;

#pragma mark - Sorting Metadata

/// The title sort order
@property (nonatomic, nullable) NSString *titleSortOrder;

/// The artist sort order
@property (nonatomic, nullable) NSString *artistSortOrder;

/// The album title sort order
@property (nonatomic, nullable) NSString *albumTitleSortOrder;

/// The album artist sort order
@property (nonatomic, nullable) NSString *albumArtistSortOrder;

/// The composer sort order
@property (nonatomic, nullable) NSString *composerSortOrder;

/// The genre sort order
@property (nonatomic, nullable) NSString *genreSortOrder;

#pragma mark - Grouping Metadata

/// The grouping
@property (nonatomic, nullable) NSString *grouping;

#pragma mark - Additional Metadata

/// The additional metadata
@property (nonatomic, nullable) NSDictionary *additionalMetadata;

#pragma mark - ReplayGain Metadata

/// The replay gain reference loudness
@property (nonatomic, nullable) NSNumber *replayGainReferenceLoudness NS_REFINED_FOR_SWIFT;

/// The replay gain track gain
@property (nonatomic, nullable) NSNumber *replayGainTrackGain NS_REFINED_FOR_SWIFT;

/// The replay gain track peak
@property (nonatomic, nullable) NSNumber *replayGainTrackPeak NS_REFINED_FOR_SWIFT;

/// The replay gain album gain
@property (nonatomic, nullable) NSNumber *replayGainAlbumGain NS_REFINED_FOR_SWIFT;

/// The replay gain album peak
@property (nonatomic, nullable) NSNumber *replayGainAlbumPeak NS_REFINED_FOR_SWIFT;

#pragma mark - Metadata Utilities

/// Copies all metadata from \c metadata
/// @note Does not copy album artwork
/// @param metadata A \c Metadata object containing the metadata to copy
/// @see -copyMetadataOfKind:from:
/// @see -copyAttachedPictures:
- (void)copyMetadataFrom:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyMetadata(from:));

/// Copies the values contained in the specified metadata kinds from \c metadata
/// @note Does not copy album artwork
/// @param metadata A \c Metadata object containing the metadata to copy
/// @param kind A bitmask specifying the kinds of metadata to copy
/// @see -copyMetadataFrom:
/// @see -copyAttachedPictures
- (void)copyMetadataOfKind:(SFBAudioMetadataKind)kind from:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyMetadata(ofKind:from:));

/// Sets the values contained in specified metadata kinds to \c nullptr
/// @param kind A bitmask specifying the kinds of metadata to remove
/// @see -removeAllMetadata
/// @see -removeAllAttachedPictures
- (void)removeMetadataOfKind:(SFBAudioMetadataKind)kind NS_SWIFT_NAME(removeMetadata(ofKind:));

/// Sets all metadata to \c nullptr
/// @note Leaves album art intact
/// @see -removeMetadataOfKind:
/// @see -removeAllAttachedPictures
- (void)removeAllMetadata;

#pragma mark - Attached Pictures

/// Get all attached pictures
@property (nonatomic, readonly) NSSet<SFBAttachedPicture *> *attachedPictures;

#pragma mark - Attached Picture Utilities

/// Copies album artwork from \c metadata
/// @note This clears existing album artwork
/// @note Does not copy metadata
/// @param metadata A \c SFBAudioMetadata object containing the artwork to copy
/// @see -copyMetadataFrom:
- (void)copyAttachedPicturesFrom:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyAttachedPicturesFrom(_:));

/// Get all attached pictures of the specified type
- (NSArray<SFBAttachedPicture *> *)attachedPicturesOfType:(SFBAttachedPictureType)type NS_SWIFT_NAME(attachedPictures(ofType:));

/// Attach a picture
- (void)attachPicture:(SFBAttachedPicture *)picture NS_SWIFT_NAME(attachPicture(_:));

/// Remove an attached picture
- (void)removeAttachedPicture:(SFBAttachedPicture *)picture NS_SWIFT_NAME(removeAttachedPicture(_:));

/// Remove all attached pictures of the specified type
- (void)removeAttachedPicturesOfType:(SFBAttachedPictureType)type NS_SWIFT_NAME(removeAttachedPicturesOfType(_:));

/// Remove all attached pictures
- (void)removeAllAttachedPictures;

#pragma mark - External Representation

/// Copy the values contained in this object to a dictionary
/// @return A dictionary containing this object's metadata and attached pictures
@property (nonatomic, readonly) NSDictionary<SFBAudioMetadataKey, id> *dictionaryRepresentation;

/// Sets the metadata and attached pictures contained in this object from a dictionary
/// @param dictionary A dictionary containing the desired values
- (void)setFromDictionaryRepresentation:(NSDictionary<SFBAudioMetadataKey, id> *)dictionary NS_SWIFT_NAME(setFrom(_:));

#pragma mark - Dictionary-Like Interface

/// Returns the metadata value for a key
/// @param key The key for the desired metadata value
/// @return The metadata value for \c key
- (nullable id)objectForKey:(SFBAudioMetadataKey)key;
/// Sets the metadata value for a key
/// @param obj The metadata value to set
/// @param key The key for the metadata value
- (void)setObject:(id)obj forKey:(SFBAudioMetadataKey)key;
/// Removes the metadata value for a key
/// @param key The key for the metadata value to remove
- (void)removeObjectForKey:(SFBAudioMetadataKey)key;

/// Returns the metadata value for a key
/// @param key The key for the desired metadata value
/// @return The metadata value for \c key
- (nullable id)valueForKey:(SFBAudioMetadataKey)key;
/// Sets or removes a metadata value
/// @param obj The metadata value to set or \c nil to remove
/// @param key The key for the metadata value
- (void)setValue:(nullable id)obj forKey:(SFBAudioMetadataKey)key;

/// Returns the metadata value for a key
/// @param key The key for the desired metadata value
/// @return The metadata value for \c key
- (nullable id)objectForKeyedSubscript:(SFBAudioMetadataKey)key;
/// Sets or removes a metadata value
/// @param obj The metadata value to set or \c nil to remove
/// @param key The key for the metadata value
- (void)setObject:(nullable id)obj forKeyedSubscript:(SFBAudioMetadataKey)key;

@end

NS_ASSUME_NONNULL_END
