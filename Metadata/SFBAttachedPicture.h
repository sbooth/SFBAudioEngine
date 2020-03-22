/*
 * Copyright (c) 2012 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*! @name \c SFBAttachedPicture Dictionary Keys */
//@{
extern NSString * const SFBAttachedPictureImageDataKey;		/*!< @brief Raw image data (\c NSData) */
extern NSString * const SFBAttachedPictureTypeKey;			/*!< @brief Picture type (\c NSNumber) */
extern NSString * const SFBAttachedPictureDescriptionKey;	/*!< @brief Picture description (\c NSString) */
//@}


/*! @brief The function or content of a picture */
typedef NS_ENUM(NSUInteger, SFBAttachedPictureType) {
	SFBAttachedPictureTypeOther					= 0x00,		/*!< A type not otherwise enumerated */
	SFBAttachedPictureTypeFileIcon				= 0x01,		/*!< 32x32 PNG image that should be used as the file icon */
	SFBAttachedPictureTypeOtherFileIcon			= 0x02,		/*!< File icon of a different size or format */
	SFBAttachedPictureTypeFrontCover			= 0x03,		/*!< Front cover image of the album */
	SFBAttachedPictureTypeBackCover				= 0x04,		/*!< Back cover image of the album */
	SFBAttachedPictureTypeLeafletPage			= 0x05,		/*!< Inside leaflet page of the album */
	SFBAttachedPictureTypeMedia					= 0x06,		/*!< Image from the album itself */
	SFBAttachedPictureTypeLeadArtist			= 0x07,		/*!< Picture of the lead artist or soloist */
	SFBAttachedPictureTypeArtist				= 0x08,		/*!< Picture of the artist or performer */
	SFBAttachedPictureTypeConductor				= 0x09,		/*!< Picture of the conductor */
	SFBAttachedPictureTypeBand					= 0x0A,		/*!< Picture of the band or orchestra */
	SFBAttachedPictureTypeComposer				= 0x0B,		/*!< Picture of the composer */
	SFBAttachedPictureTypeLyricist				= 0x0C,		/*!< Picture of the lyricist or text writer */
	SFBAttachedPictureTypeRecordingLocation		= 0x0D,		/*!< Picture of the recording location or studio */
	SFBAttachedPictureTypeDuringRecording		= 0x0E,		/*!< Picture of the artists during recording */
	SFBAttachedPictureTypeDuringPerformance		= 0x0F,		/*!< Picture of the artists during performance */
	SFBAttachedPictureTypeMovieScreenCapture	= 0x10,		/*!< Picture from a movie or video related to the track */
	SFBAttachedPictureTypeColouredFish			= 0x11,		/*!< Picture of a large, coloured fish */
	SFBAttachedPictureTypeIllustration			= 0x12,		/*!< Illustration related to the track */
	SFBAttachedPictureTypeBandLogo				= 0x13,		/*!< Logo of the band or performer */
	SFBAttachedPictureTypePublisherLogo			= 0x14		/*!< Logo of the publisher (record company) */
};


/*!
 * @brief A class encapsulating a single attached picture.
 *
 * Most file formats may have more than one attached picture of each type.
 */
@interface SFBAttachedPicture : NSObject

/*!
 * @brief Create a new \c SFBAttachedPicture from the values contained in a dictionary
 * @param dictionary A dictionary containing the desired values
 */
+ (instancetype)attachedPictureFromDictionaryRepresentation:(NSDictionary *)dictionary;

/*!
 * @brief Create a new \c SFBAttachedPicture
 * @param imageData The raw image data
 */
- (instancetype)initWithImageData:(NSData *)imageData;

/*!
 * @brief Create a new \c SFBAttachedPicture
 * @param imageData The raw image data
 * @param type The  artwork type
 */
- (instancetype)initWithImageData:(NSData *)imageData type:(SFBAttachedPictureType)type;

/*!
 * @brief Create a new \c SFBAttachedPicture
 * @param imageData The raw image data
 * @param type The  artwork type
 * @param description The  image description
 */
- (instancetype)initWithImageData:(NSData *)imageData type:(SFBAttachedPictureType)type description:(nullable NSString *)description;


/*!
 * @brief Copy the values contained in this object to a dictionary
 * @return A dictionary containing this object's artwork information
 */
- (NSDictionary *)dictionaryRepresentation;


/*! @brief The raw image data */
@property (nonatomic, readonly) NSData *imageData;

/*! @brief The artwork type */
@property (nonatomic, readonly) SFBAttachedPictureType pictureType;

/*! @brief The image description */
@property (nonatomic, nullable, readonly) NSString *pictureDescription;

@end

NS_ASSUME_NONNULL_END
