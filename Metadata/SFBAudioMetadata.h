/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

#import "SFBAttachedPicture.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief The \c CFErrorRef error domain used by \c SFBAudioMetadata and subclasses */
extern NSErrorDomain const SFBAudioMetadataErrorDomain;


/*! @brief Possible \c CFErrorRef error codes used by \c SFBAudioMetadata */
typedef NS_ENUM(NSUInteger, SFBAudioMetadataErrorCode) {
	SFBAudioMetadataErrorCodeFileFormatNotRecognized		= 0,	/*!< File format not recognized */
	SFBAudioMetadataErrorCodeFileFormatNotSupported			= 1,	/*!< File format not supported */
	SFBAudioMetadataErrorCodeInputOutput					= 2		/*!< Input/output error */
};


/*! @brief Metadata kind bitmask values used in CopyMetadata() and RemoveMetadata() */
typedef NS_OPTIONS(NSUInteger, SFBAudioMetadataKind) {
	SFBAudioMetadataKindBasic			= (1u << 0),	/*!< Basic metadata */
	SFBAudioMetadataKindSorting			= (1u << 1),	/*!< Sorting metadata */
	SFBAudioMetadataKindGrouping		= (1u << 2),	/*!< Grouping metadata */
	SFBAudioMetadataKindAdditional		= (1u << 3),	/*!< Additional metadata */
	SFBAudioMetadataKindReplayGain		= (1u << 4)		/*!< Replay gain metadata */
};


/*! @name Audio property dictionary keys */
//@{
extern NSString * const SFBAudioMetadataFormatNameKey;					/*!< @brief The name of the audio format */
extern NSString * const SFBAudioMetadataTotalFramesKey;					/*!< @brief The total number of audio frames (\c NSNumber) */
extern NSString * const SFBAudioMetadataChannelsPerFrameKey;			/*!< @brief The number of channels (\c NSNumber) */
extern NSString * const SFBAudioMetadataBitsPerChannelKey;				/*!< @brief The number of bits per channel (\c NSNumber) */
extern NSString * const SFBAudioMetadataSampleRateKey;					/*!< @brief The sample rate (\c NSNumber) */
extern NSString * const SFBAudioMetadataDurationKey;					/*!< @brief The duration (\c NSNumber) */
extern NSString * const SFBAudioMetadataBitrateKey;						/*!< @brief The audio bitrate (\c NSNumber) */
//@}

/*! @name Metadata dictionary keys */
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


/*! @brief Base class for all audio metadata reader/writer classes */
@interface SFBAudioMetadata : NSObject

/*! @brief Returns an array containing the supported file extensions */
+ (NSArray<NSString *> *)supportedFileExtensions;

/*!@brief Returns  an array containing the supported MIME types */
+ (NSArray<NSString *> *)supportedMIMETypes;


/*! @brief Tests whether a file extension is supported */
+ (BOOL)handlesFilesWithExtension:(NSString *)extension;

/*! @brief Tests whether a MIME type is supported */
+ (BOOL)handlesMIMEType:(NSString *)mimeType;


/*!
 * @brief Create a new \c SFBAudioMetadata  for the specified URL
 * @param url The URL
 * @param error An optional pointer to a \c NSError to receive error information
 * @return A \c SFBAudioMetadata object, or \c nil on failure
 */
+ (instancetype)metadataForURL:(NSURL *)url error:(NSError * _Nullable *)error;

/*!
 * @brief Create a new \c SFBAudioMetadata  from the values contained in a dictionary
 * @param dictionary A dictionary containing the desired values
 */
+ (instancetype)metadataFromDictionaryRepresentation:(NSDictionary *)dictionary;


/*! @brief The URL containing this metadata */
@property (nonatomic) NSURL *url;


/*!
 * @brief Reads the metadata
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL) readMetadata:(NSError * _Nullable *)error;

/*!
 * @brief Writes the metadata
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL) writeMetadata:(NSError * _Nullable *)error;


/*!
 * @brief Copy the values contained in this object to a dictionary
 * @return A dictionary containing this object's artwork information
 */
- (NSDictionary *)dictionaryRepresentation;


/*! @brief Query the object for unmerged changes */
- (BOOL)hasChanges;

/*! @brief Merge changes */
- (void)mergeChanges;

/*! @brief Revert unmerged changes */
- (void)revertChanges;


/*!
 * @brief Copies all metadata from \c metadata
 * @note Does not copy album artwork
 * @param metadata A \c Metadata object containing the metadata to copy
 * @see -copyMetadataOfKind:from:
 * @see -copyAttachedPictures:
 */
- (void)copyMetadataFrom:(SFBAudioMetadata *)metadata;

/*!
 * @brief Copies the values contained in the specified metadata kinds from \c metadata
 * @note Does not copy album artwork
 * @param metadata A \c Metadata object containing the metadata to copy
 * @param kind A bitmask specifying the kinds of metadata to copy
 * @see -copyMetadataFrom:
 * @see -copyAttachedPictures
 */
- (void)copyMetadataOfKind:(SFBAudioMetadataKind)kind from:(SFBAudioMetadata *)metadata;

/*!
 * @brief Sets the values contained in specified metadata kinds to \c nullptr
 * @param kind A bitmask specifying the kinds of metadata to remove
 * @see -removeAllMetadata
 * @see -removeAllAttachedPictures
 */
- (void)removeMetadataOfKind:(SFBAudioMetadataKind)kind;

/*!
 * @brief Sets all metadata to \c nullptr
 * @note Leaves album art intact
 * @see -removeMetadataOfKind:
 * @see -removeAllAttachedPictures
 */
- (void)removeAllMetadata;


/*! @brief Get the name of the audio format */
@property (nonatomic, nullable, readonly) NSString *formatName;

/*! @brief Get the total number of audio frames */
@property (nonatomic, nullable, readonly) NSNumber *totalFrames;

/*! @brief Get the number of channels */
@property (nonatomic, nullable, readonly) NSNumber *channelsPerFrame;

/*! @brief Get the number of bits per channel */
@property (nonatomic, nullable, readonly) NSNumber *bitsPerChannel;

/*! @brief Get the sample rate in Hz */
@property (nonatomic, nullable, readonly) NSNumber *sampleRate;

/*! @brief Get the duration in seconds */
@property (nonatomic, nullable, readonly) NSNumber *duration;

/*! @brief Get the audio bitrate in KiB/sec */
@property (nonatomic, nullable, readonly) NSNumber *bitrate;


/*! @brief Get the title */
@property (nonatomic, nullable) NSString *title;

/*! @brief Get the album title */
@property (nonatomic, nullable) NSString *albumTitle;

/*! @brief Get the artist */
@property (nonatomic, nullable) NSString *artist;

/*! @brief Get the album artist */
@property (nonatomic, nullable) NSString *albumArtist;

/*! @brief Get the genre */
@property (nonatomic, nullable) NSString *genre;

/*! @brief Get the composer */
@property (nonatomic, nullable) NSString *composer;

/*! @brief Get the release date */
@property (nonatomic, nullable) NSString *releaseDate;

/*! @brief Get the compilation flag */
@property (nonatomic, nullable) NSNumber *compilation;

/*! @brief Get the track number */
@property (nonatomic, nullable) NSNumber *trackNumber;

/*! @brief Get the track total */
@property (nonatomic, nullable) NSNumber *trackTotal;

/*! @brief Get the disc number */
@property (nonatomic, nullable) NSNumber *discNumber;

/*! @brief Get the disc total */
@property (nonatomic, nullable) NSNumber *discTotal;

/*! @brief Get the lyrics */
@property (nonatomic, nullable) NSString *lyrics;

/*! @brief Get the Beats per minute (BPM) */
@property (nonatomic, nullable) NSNumber *bpm;

/*! @brief Get the rating */
@property (nonatomic, nullable) NSNumber *rating;

/*! @brief Get the comment */
@property (nonatomic, nullable) NSString *comment;

/*! @brief Get the Media Catalog Number (MCN) */
@property (nonatomic, nullable) NSString *mcn;

/*! @brief Get the International Standard Recording Code (ISRC) */
@property (nonatomic, nullable) NSString *isrc;

/*! @brief Get the MusicBrainz release ID */
@property (nonatomic, nullable) NSString *musicBrainzReleaseID;

/*! @brief Get the MusicBrainz recording ID */
@property (nonatomic, nullable) NSString *musicBrainzRecordingID;


/*! @brief Get the title sort order */
@property (nonatomic, nullable) NSString *titleSortOrder;

/*! @brief Get the album title sort order */
@property (nonatomic, nullable) NSString *albumTitleSortOrder;

/*! @brief Get the artist sort order */
@property (nonatomic, nullable) NSString *artistSortOrder;

/*! @brief Get the album artist sort order */
@property (nonatomic, nullable) NSString *albumArtistSortOrder;

/*! @brief Get the composer sort order */
@property (nonatomic, nullable) NSString *composerSortOrder;


/*! @brief Get the grouping */
@property (nonatomic, nullable) NSString *grouping;


/*! @brief Get the additional metadata */
@property (nonatomic, nullable) NSDictionary *additionalMetadata;


/*! @brief Get the replay gain reference loudness */
@property (nonatomic, nullable) NSNumber *replayGainReferenceLoudness;

/*! @brief Get the replay gain track gain */
@property (nonatomic, nullable) NSNumber *replayGainTrackGain;

/*! @brief Get the replay gain track peak */
@property (nonatomic, nullable) NSNumber *replayGainTrackPeak;

/*! @brief Get the replay gain album gain */
@property (nonatomic, nullable) NSNumber *replayGainAlbumGain;

/*! @brief Get the replay gain album peak */
@property (nonatomic, nullable) NSNumber *replayGainAlbumPeak;


/*!
 * @brief Copies album artwork from \c metadata
 * @note This clears existing album artwork
 * @note Does not copy metadata
 * @param metadata A \c SFBAudioMetadata object containing the artwork to copy
 * @see -copyMetadataFrom:
 */
- (void)copyAttachedPicturesFrom:(SFBAudioMetadata *)metadata;

/*! @brief Get all attached pictures */
- (NSArray<SFBAttachedPicture *> *)attachedPictures;

/*! @brief Get all attached pictures of the specified type */
- (NSArray<SFBAttachedPicture *> *)attachedPicturesOfType:(SFBAttachedPictureType)type;

/*! @brief Attach a picture */
- (void)attachPicture:(SFBAttachedPicture *)picture;

/*! @brief Remove an attached picture */
- (void)removePicture:(SFBAttachedPicture *)picture;

/*! @brief Remove all attached pictures of the specified type */
- (void)removeAttachedPicturesOfType:(SFBAttachedPictureType)type;

/*! @brief Remove all attached pictures */
- (void)removeAllAttachedPictures;

@end

NS_ASSUME_NONNULL_END
