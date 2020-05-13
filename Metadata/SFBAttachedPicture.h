/*
 * Copyright (c) 2012 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// \c SFBAttachedPicture Dictionary Keys
typedef NSString * SFBAttachedPictureKey NS_TYPED_ENUM NS_SWIFT_NAME(AttachedPicture.Key);
/// Raw image data (\c NSData)
extern SFBAttachedPictureKey const SFBAttachedPictureKeyImageData;
/// Picture type (\c NSNumber)
extern SFBAttachedPictureKey const SFBAttachedPictureKeyType;
/// Picture description (\c NSString)
extern SFBAttachedPictureKey const SFBAttachedPictureKeyDescription;

/// The function or content of a picture
typedef NS_ENUM(NSUInteger, SFBAttachedPictureType) {
	/// A type not otherwise enumerated
	SFBAttachedPictureTypeOther					= 0x00,
	/// 32x32 PNG image that should be used as the file icon
	SFBAttachedPictureTypeFileIcon				= 0x01,
	/// File icon of a different size or format
	SFBAttachedPictureTypeOtherFileIcon			= 0x02,
	/// Front cover image of the album
	SFBAttachedPictureTypeFrontCover			= 0x03,
	/// Back cover image of the album
	SFBAttachedPictureTypeBackCover				= 0x04,
	/// Inside leaflet page of the album
	SFBAttachedPictureTypeLeafletPage			= 0x05,
	/// Image from the album itself
	SFBAttachedPictureTypeMedia					= 0x06,
	/// Picture of the lead artist or soloist
	SFBAttachedPictureTypeLeadArtist			= 0x07,
	/// Picture of the artist or performer
	SFBAttachedPictureTypeArtist				= 0x08,
	/// Picture of the conductor
	SFBAttachedPictureTypeConductor				= 0x09,
	/// Picture of the band or orchestra
	SFBAttachedPictureTypeBand					= 0x0A,
	/// Picture of the composer
	SFBAttachedPictureTypeComposer				= 0x0B,
	/// Picture of the lyricist or text writer
	SFBAttachedPictureTypeLyricist				= 0x0C,
	/// Picture of the recording location or studio
	SFBAttachedPictureTypeRecordingLocation		= 0x0D,
	/// Picture of the artists during recording
	SFBAttachedPictureTypeDuringRecording		= 0x0E,
	/// Picture of the artists during performance
	SFBAttachedPictureTypeDuringPerformance		= 0x0F,
	/// Picture from a movie or video related to the track
	SFBAttachedPictureTypeMovieScreenCapture	= 0x10,
	/// Picture of a large, coloured fish
	SFBAttachedPictureTypeColouredFish			= 0x11,
	/// Illustration related to the track
	SFBAttachedPictureTypeIllustration			= 0x12,
	/// Logo of the band or performer
	SFBAttachedPictureTypeBandLogo				= 0x13,
	/// Logo of the publisher (record company)
	SFBAttachedPictureTypePublisherLogo			= 0x14
} NS_SWIFT_NAME(AttachedPicture.Type);

/// A class encapsulating a single attached picture.
///
/// Most file formats may have more than one attached picture of each type.
NS_SWIFT_NAME(AttachedPicture) @interface SFBAttachedPicture : NSObject <NSCopying>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAttachedPicture object
/// @param imageData The raw image data
- (instancetype)initWithImageData:(NSData *)imageData;

/// Returns an initialized \c SFBAttachedPicture object
/// @param imageData The raw image data
/// @param type The artwork type
- (instancetype)initWithImageData:(NSData *)imageData type:(SFBAttachedPictureType)type;

/// Returns an initialized \c SFBAttachedPicture object
/// @param imageData The raw image data
/// @param type The artwork type
/// @param description The image description
- (instancetype)initWithImageData:(NSData *)imageData type:(SFBAttachedPictureType)type description:(nullable NSString *)description NS_DESIGNATED_INITIALIZER;

/// Returns an initialized \c SFBAttachedPicture object or \c nil on error
///
/// Returns \c nil if \c dictionaryRepresentation does not contain image data
/// @param dictionaryRepresentation A dictionary containing the desired values
- (nullable instancetype)initWithDictionaryRepresentation:(NSDictionary<SFBAttachedPictureKey, id> *)dictionaryRepresentation;


/// Copy the values contained in this object to a dictionary
/// @return A dictionary containing this object's artwork information
@property (nonatomic, readonly) NSDictionary<SFBAttachedPictureKey, id> *dictionaryRepresentation;


/// The raw image data
@property (nonatomic, readonly) NSData *imageData;

/// The artwork type
@property (nonatomic, readonly) SFBAttachedPictureType pictureType NS_SWIFT_NAME(type);

/// The artwork description
@property (nonatomic, nullable, readonly) NSString *pictureDescription NS_SWIFT_NAME(description);

@end

NS_ASSUME_NONNULL_END
