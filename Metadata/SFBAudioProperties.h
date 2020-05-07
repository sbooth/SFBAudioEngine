/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NSString * SFBAudioPropertiesKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioProperties.Key);

// Audio property dictionary keys
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyFormatName;				///< The name of the audio format
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyTotalFrames;			///< The total number of audio frames (\c NSNumber)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyChannelsPerFrame;		///< The number of channels (\c NSNumber)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitsPerChannel;			///< The number of bits per channel (\c NSNumber)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeySampleRate;				///< The sample rate (\c NSNumber)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyDuration;				///< The duration (\c NSNumber)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitrate;				///< The audio bitrate (\c NSNumber)

/// Class providing information on basic audio properties
NS_SWIFT_NAME(AudioProperties) @interface SFBAudioProperties : NSObject <NSCopying>

/// Returns an initialized an \c SFBAudioProperties object
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/// Returns an initialized an \c SFBAudioProperties object populated with values from \c dictionaryRepresentation
/// @param dictionaryRepresentation A dictionary containing the desired values
- (instancetype)initWithDictionaryRepresentation:(NSDictionary<SFBAudioPropertiesKey, id> *)dictionaryRepresentation NS_DESIGNATED_INITIALIZER;

/// The name of the audio format
@property (nonatomic, nullable, readonly) NSString *formatName;

/// The total number of audio frames
@property (nonatomic, nullable, readonly) NSNumber *totalFrames;

/// The number of channels
@property (nonatomic, nullable, readonly) NSNumber *channelsPerFrame;

/// The number of bits per channel
@property (nonatomic, nullable, readonly) NSNumber *bitsPerChannel;

/// The sample rate in Hz
@property (nonatomic, nullable, readonly) NSNumber *sampleRate;

/// The duration in seconds
@property (nonatomic, nullable, readonly) NSNumber *duration;

/// The audio bitrate in KiB/sec
@property (nonatomic, nullable, readonly) NSNumber *bitrate;

/// A dictionary containing the audio properties
@property (nonatomic, readonly) NSDictionary<SFBAudioPropertiesKey, id> *dictionaryRepresentation;

@end

NS_ASSUME_NONNULL_END
