//
// Copyright (c) 2006-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// A key in an audio properties dictionary
typedef NSString * SFBAudioPropertiesKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioProperties.Key);

// Audio property dictionary keys
/// The name of the audio format
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyFormatName;
/// The total number of audio frames (`NSNumber`)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyFrameLength;
/// The number of channels (`NSNumber`)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyChannelCount;
/// The audio bit depth (`NSNumber`)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitDepth;
/// The sample rate in Hz (`NSNumber`)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeySampleRate;
/// The duration in seconds (`NSNumber`)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyDuration;
/// The audio bitrate in KiB/sec (`NSNumber`)
extern SFBAudioPropertiesKey const SFBAudioPropertiesKeyBitrate;

/// Class providing information on basic audio properties
NS_SWIFT_NAME(AudioProperties) @interface SFBAudioProperties : NSObject <NSCopying>

/// Returns an initialized an `SFBAudioProperties` object
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/// Returns an initialized an `SFBAudioProperties` object populated with values from `dictionaryRepresentation`
/// - parameter dictionaryRepresentation: A dictionary containing the desired values
- (instancetype)initWithDictionaryRepresentation:(NSDictionary<SFBAudioPropertiesKey, id> *)dictionaryRepresentation NS_DESIGNATED_INITIALIZER;

/// The name of the audio format
@property (nonatomic, nullable, readonly) NSString *formatName;

/// The total number of audio frames
@property (nonatomic, nullable, readonly) NSNumber *frameLength NS_REFINED_FOR_SWIFT;

/// The number of channels
@property (nonatomic, nullable, readonly) NSNumber *channelCount NS_REFINED_FOR_SWIFT;

/// The audio bit depth
@property (nonatomic, nullable, readonly) NSNumber *bitDepth NS_REFINED_FOR_SWIFT;

/// The sample rate in Hz
@property (nonatomic, nullable, readonly) NSNumber *sampleRate NS_REFINED_FOR_SWIFT;

/// The duration in seconds
@property (nonatomic, nullable, readonly) NSNumber *duration NS_REFINED_FOR_SWIFT;

/// The audio bitrate in KiB/sec
@property (nonatomic, nullable, readonly) NSNumber *bitrate NS_REFINED_FOR_SWIFT;

#pragma mark - External Representation

/// A dictionary containing the audio properties
@property (nonatomic, readonly) NSDictionary<SFBAudioPropertiesKey, id> *dictionaryRepresentation;

#pragma mark - Dictionary-Like Interface

/// Returns the property value for a key
/// - parameter key: The key for the desired property value
/// - returns: The property value for `key`
- (nullable id)objectForKey:(SFBAudioPropertiesKey)key;

/// Returns the property value for a key
/// - parameter key: The key for the desired property value
/// - returns: The property value for `key`
- (nullable id)valueForKey:(SFBAudioPropertiesKey)key;

/// Returns the property value for a key
/// - parameter key: The key for the desired property value
/// - returns: The property value for `key`
- (nullable id)objectForKeyedSubscript:(SFBAudioPropertiesKey)key;

@end

NS_ASSUME_NONNULL_END
