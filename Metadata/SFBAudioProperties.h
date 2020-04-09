/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*! @name Audio property dictionary keys */
typedef NSString * SFBAudioPropertiesKey NS_TYPED_ENUM;
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyFormatName;				/*!< @brief The name of the audio format */
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyTotalFrames;			/*!< @brief The total number of audio frames (\c NSNumber) */
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyChannelsPerFrame;		/*!< @brief The number of channels (\c NSNumber) */
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitsPerChannel;			/*!< @brief The number of bits per channel (\c NSNumber) */
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeySampleRate;				/*!< @brief The sample rate (\c NSNumber) */
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyDuration;				/*!< @brief The duration (\c NSNumber) */
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitrate;				/*!< @brief The audio bitrate (\c NSNumber) */

/*! @brief Class providing information on basic audio properties  */
@interface SFBAudioProperties : NSObject <NSCopying>

/*! @brief Returns an initialized an \c SFBAudioProperties object */
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/*!
 * @brief Returns an initialized an \c SFBAudioProperties object populated with values from \c dictionaryRepresentation
 * @param dictionaryRepresentation A dictionary containing the desired values
 */
- (instancetype)initWithDictionaryRepresentation:(NSDictionary<SFBAudioPropertiesKey, id> *)dictionaryRepresentation NS_DESIGNATED_INITIALIZER;

/*! @brief The name of the audio format */
@property (nonatomic, nullable, readonly) NSString *formatName;

/*! @brief The total number of audio frames */
@property (nonatomic, nullable, readonly) NSNumber *totalFrames;

/*! @brief The number of channels */
@property (nonatomic, nullable, readonly) NSNumber *channelsPerFrame;

/*! @brief The number of bits per channel */
@property (nonatomic, nullable, readonly) NSNumber *bitsPerChannel;

/*! @brief The sample rate in Hz */
@property (nonatomic, nullable, readonly) NSNumber *sampleRate;

/*! @brief The duration in seconds */
@property (nonatomic, nullable, readonly) NSNumber *duration;

/*! @brief The audio bitrate in KiB/sec */
@property (nonatomic, nullable, readonly) NSNumber *bitrate;

/*! @brief A dictionary containing the audio properties */
@property (nonatomic, readonly) NSDictionary<SFBAudioPropertiesKey, id> *dictionaryRepresentation;

@end

NS_ASSUME_NONNULL_END
