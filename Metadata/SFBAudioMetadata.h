/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

#import "SFBAttachedPicture.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief Metadata kind bitmask values used in copyMetadataOfKind:from: and removeMetadataOfKind: */
typedef NS_OPTIONS(NSUInteger, SFBAudioMetadataKind) {
	SFBAudioMetadataKindBasic			= (1u << 0),	/*!< Basic metadata */
	SFBAudioMetadataKindSorting			= (1u << 1),	/*!< Sorting metadata */
	SFBAudioMetadataKindGrouping		= (1u << 2),	/*!< Grouping metadata */
	SFBAudioMetadataKindAdditional		= (1u << 3),	/*!< Additional metadata */
	SFBAudioMetadataKindReplayGain		= (1u << 4)		/*!< Replay gain metadata */
};

/*! @name Basic metadata dictionary keys */
//@{
extern NSString * const SFBAudioMetadataTitleKey;						/*!< @brief Title (\c NSString) */
extern NSString * const SFBAudioMetadataAlbumTitleKey;					/*!< @brief Album title (\c NSString) */
extern NSString * const SFBAudioMetadataArtistKey;						/*!< @brief Artist (\c NSString) */
extern NSString * const SFBAudioMetadataAlbumArtistKey;					/*!< @brief Album artist (\c NSString) */
extern NSString * const SFBAudioMetadataGenreKey;						/*!< @brief Genre (\c NSString) */
extern NSString * const SFBAudioMetadataComposerKey;					/*!< @brief Composer (\c NSString) */
extern NSString * const SFBAudioMetadataReleaseDateKey;					/*!< @brief Release date (\c NSString) */
extern NSString * const SFBAudioMetadataCompilationKey;					/*!< @brief Compilation flag (\c NSNumber) */
extern NSString * const SFBAudioMetadataTrackNumberKey;					/*!< @brief Track number (\c NSNumber) */
extern NSString * const SFBAudioMetadataTrackTotalKey;					/*!< @brief Track total (\c NSNumber) */
extern NSString * const SFBAudioMetadataDiscNumberKey;					/*!< @brief Disc number (\c NSNumber) */
extern NSString * const SFBAudioMetadataDiscTotalKey;					/*!< @brief Disc total (\c NSNumber) */
extern NSString * const SFBAudioMetadataLyricsKey;						/*!< @brief Lyrics (\c NSString) */
extern NSString * const SFBAudioMetadataBPMKey;							/*!< @brief Beats per minute (BPM) (\c NSNumber) */
extern NSString * const SFBAudioMetadataRatingKey;						/*!< @brief Rating (\c NSNumber) */
extern NSString * const SFBAudioMetadataCommentKey;						/*!< @brief Comment (\c NSString) */
extern NSString * const SFBAudioMetadataISRCKey;						/*!< @brief International Standard Recording Code (ISRC) (\c NSString) */
extern NSString * const SFBAudioMetadataMCNKey;							/*!< @brief Media Catalog Number (MCN) (\c NSString) */
extern NSString * const SFBAudioMetadataMusicBrainzReleaseIDKey;		/*!< @brief MusicBrainz release ID (\c NSString) */
extern NSString * const SFBAudioMetadataMusicBrainzRecordingIDKey;		/*!< @brief MusicBrainz recording ID (\c NSString) */
//@}

/*! @name Sorting dictionary keys */
//@{
extern NSString * const SFBAudioMetadataTitleSortOrderKey;				/*!< @brief Title sort order (\c NSString) */
extern NSString * const SFBAudioMetadataAlbumTitleSortOrderKey;			/*!< @brief Album title sort order (\c NSString) */
extern NSString * const SFBAudioMetadataArtistSortOrderKey;				/*!< @brief Artist sort order (\c NSString) */
extern NSString * const SFBAudioMetadataAlbumArtistSortOrderKey;		/*!< @brief Album artist sort order (\c NSString) */
extern NSString * const SFBAudioMetadataComposerSortOrderKey;			/*!< @brief Composer sort order (\c NSString) */
extern NSString * const SFBAudioMetadataGenreSortOrderKey;				/*!< @brief Genre sort order (\c NSString) */
//@}

/*! @name Grouping dictionary keys */
//@{
extern NSString * const SFBAudioMetadataGroupingKey;					/*!< @brief Grouping (\c NSString) */
//@}

/*! @name Additional metadata dictionary keys */
//@{
extern NSString * const SFBAudioMetadataAdditionalMetadataKey;			/*!< @brief Additional metadata (\c NSDictionary) */
//@}

/*! @name Replay gain dictionary keys */
//@{
extern NSString * const SFBAudioMetadataReplayGainReferenceLoudnessKey;	/*!< @brief Replay gain reference loudness (\c NSNumber) */
extern NSString * const SFBAudioMetadataReplayGainTrackGainKey;			/*!< @brief Replay gain track gain (\c NSNumber) */
extern NSString * const SFBAudioMetadataReplayGainTrackPeakKey;			/*!< @brief Replay gain track peak (\c NSNumber) */
extern NSString * const SFBAudioMetadataReplayGainAlbumGainKey;			/*!< @brief Replay gain album gain (\c NSNumber) */
extern NSString * const SFBAudioMetadataReplayGainAlbumPeakKey;			/*!< @brief Replay gain album peak (\c NSNumber) */
//@}

/*! @name Attached Picture dictionary keys */
//@{
extern NSString * const SFBAudioMetadataAttachedPicturesKey;			/*!< @brief Attached pictures (\c NSArray of \c NSDictionary) */
//@}


/*! @brief Class supporting commonly-used audio metadata and attached pictures */
@interface SFBAudioMetadata : NSObject <NSCopying>

/*! @brief Returns an initialized empty \c SFBAudioMetadata object */
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/*!
 * @brief Returns an initialized \c SFBAudioMetadata object populated with values from \c dictionaryRepresentation
 * @param dictionaryRepresentation A dictionary containing the desired values
 */
- (instancetype)initWithDictionaryRepresentation:(NSDictionary<NSString *, id> *)dictionaryRepresentation;

#pragma mark Basic Metadata

/*! @brief The title */
@property (nonatomic, nullable) NSString *title;

/*! @brief The album title */
@property (nonatomic, nullable) NSString *albumTitle;

/*! @brief The artist */
@property (nonatomic, nullable) NSString *artist;

/*! @brief The album artist */
@property (nonatomic, nullable) NSString *albumArtist;

/*! @brief The genre */
@property (nonatomic, nullable) NSString *genre;

/*! @brief The composer */
@property (nonatomic, nullable) NSString *composer;

/*! @brief The release date */
@property (nonatomic, nullable) NSString *releaseDate;

/*! @brief The compilation flag */
@property (nonatomic, nullable) NSNumber *compilation;

/*! @brief The track number */
@property (nonatomic, nullable) NSNumber *trackNumber;

/*! @brief The track total */
@property (nonatomic, nullable) NSNumber *trackTotal;

/*! @brief The disc number */
@property (nonatomic, nullable) NSNumber *discNumber;

/*! @brief The disc total */
@property (nonatomic, nullable) NSNumber *discTotal;

/*! @brief The lyrics */
@property (nonatomic, nullable) NSString *lyrics;

/*! @brief The Beats per minute (BPM) */
@property (nonatomic, nullable) NSNumber *bpm;

/*! @brief The rating */
@property (nonatomic, nullable) NSNumber *rating;

/*! @brief The comment */
@property (nonatomic, nullable) NSString *comment;

/*! @brief The Media Catalog Number (MCN) */
@property (nonatomic, nullable) NSString *mcn;

/*! @brief The International Standard Recording Code (ISRC) */
@property (nonatomic, nullable) NSString *isrc;

/*! @brief The MusicBrainz release ID */
@property (nonatomic, nullable) NSString *musicBrainzReleaseID;

/*! @brief The MusicBrainz recording ID */
@property (nonatomic, nullable) NSString *musicBrainzRecordingID;

#pragma mark Sorting Metadata

/*! @brief The title sort order */
@property (nonatomic, nullable) NSString *titleSortOrder;

/*! @brief The album title sort order */
@property (nonatomic, nullable) NSString *albumTitleSortOrder;

/*! @brief The artist sort order */
@property (nonatomic, nullable) NSString *artistSortOrder;

/*! @brief The album artist sort order */
@property (nonatomic, nullable) NSString *albumArtistSortOrder;

/*! @brief The composer sort order */
@property (nonatomic, nullable) NSString *composerSortOrder;

/*! @brief The genre sort order */
@property (nonatomic, nullable) NSString *genreSortOrder;

#pragma mark Grouping Metadata

/*! @brief The grouping */
@property (nonatomic, nullable) NSString *grouping;

#pragma mark Additional Metadata

/*! @brief The additional metadata */
@property (nonatomic, nullable) NSDictionary *additionalMetadata;

#pragma mark ReplayGain Metadata

/*! @brief The replay gain reference loudness */
@property (nonatomic, nullable) NSNumber *replayGainReferenceLoudness;

/*! @brief The replay gain track gain */
@property (nonatomic, nullable) NSNumber *replayGainTrackGain;

/*! @brief The replay gain track peak */
@property (nonatomic, nullable) NSNumber *replayGainTrackPeak;

/*! @brief The replay gain album gain */
@property (nonatomic, nullable) NSNumber *replayGainAlbumGain;

/*! @brief The replay gain album peak */
@property (nonatomic, nullable) NSNumber *replayGainAlbumPeak;

#pragma mark Metadata Utilities

/*!
 * @brief Copies all metadata from \c metadata
 * @note Does not copy album artwork
 * @param metadata A \c Metadata object containing the metadata to copy
 * @see -copyMetadataOfKind:from:
 * @see -copyAttachedPictures:
 */
- (void)copyMetadataFrom:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyMetadata(from:));

/*!
 * @brief Copies the values contained in the specified metadata kinds from \c metadata
 * @note Does not copy album artwork
 * @param metadata A \c Metadata object containing the metadata to copy
 * @param kind A bitmask specifying the kinds of metadata to copy
 * @see -copyMetadataFrom:
 * @see -copyAttachedPictures
 */
- (void)copyMetadataOfKind:(SFBAudioMetadataKind)kind from:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyMetadata(ofKind:from:));

/*!
 * @brief Sets the values contained in specified metadata kinds to \c nullptr
 * @param kind A bitmask specifying the kinds of metadata to remove
 * @see -removeAllMetadata
 * @see -removeAllAttachedPictures
 */
- (void)removeMetadataOfKind:(SFBAudioMetadataKind)kind NS_SWIFT_NAME(removeMetadata(ofKind:));

/*!
 * @brief Sets all metadata to \c nullptr
 * @note Leaves album art intact
 * @see -removeMetadataOfKind:
 * @see -removeAllAttachedPictures
 */
- (void)removeAllMetadata;

#pragma mark Attached Pictures

/*! @brief Get all attached pictures */
@property (nonatomic, readonly) NSSet<SFBAttachedPicture *> *attachedPictures;

#pragma mark Attached Picture Utilities

/*!
 * @brief Copies album artwork from \c metadata
 * @note This clears existing album artwork
 * @note Does not copy metadata
 * @param metadata A \c SFBAudioMetadata object containing the artwork to copy
 * @see -copyMetadataFrom:
 */
- (void)copyAttachedPicturesFrom:(SFBAudioMetadata *)metadata NS_SWIFT_NAME(copyAttachedPicturesFrom(_:));

/*! @brief Get all attached pictures of the specified type */
- (NSArray<SFBAttachedPicture *> *)attachedPicturesOfType:(SFBAttachedPictureType)type NS_SWIFT_NAME(attachedPictures(ofType:));

/*! @brief Attach a picture */
- (void)attachPicture:(SFBAttachedPicture *)picture NS_SWIFT_NAME(attachPicture(_:));

/*! @brief Remove an attached picture */
- (void)removeAttachedPicture:(SFBAttachedPicture *)picture NS_SWIFT_NAME(removeAttachedPicture(_:));

/*! @brief Remove all attached pictures of the specified type */
- (void)removeAttachedPicturesOfType:(SFBAttachedPictureType)type NS_SWIFT_NAME(removeAttachedPicturesOfType(_:));

/*! @brief Remove all attached pictures */
- (void)removeAllAttachedPictures;

#pragma mark External Representation

/*!
 * @brief Copy the values contained in this object to a dictionary
 * @return A dictionary containing this object's metadata and attached pictures
 */
@property (nonatomic, readonly) NSDictionary<NSString *, id> *dictionaryRepresentation;

/*!
 * @brief Sets the metadata and attached pictures contained in this object from a dictionary
 * @param dictionary A dictionary containing the desired values
 */
- (void)setFromDictionaryRepresentation:(NSDictionary<NSString *, id> *)dictionary NS_SWIFT_NAME(setFrom(_:));

@end

NS_ASSUME_NONNULL_END
