//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBPCMEncoding.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio converter converts audio from one format to another through a PCM intermediate format.
///
/// An audio converter reads PCM audio from an audio decoder in the decoder's processing format,
/// converts that audio to an intermediate PCM format, and then writes the intermediate PCM audio to an
/// audio encoder which performs the final conversion to the desired format.
///
/// The decoder's processing format and intermediate format must both be PCM but do not have to
/// have the same sample rate, bit depth, channel count, or channel layout.
///
/// @c AVAudioConverter is used to convert from the decoder's processing format
/// to the intermediate format, performing sample rate conversion and channel mapping as required.
NS_SWIFT_NAME(AudioConverter) @interface SFBAudioConverter : NSObject

/// Converts audio and writes to the specified URL
/// @note The file type to create is inferred from the file extension of \c destinationURL
/// @param sourceURL The URL to convert
/// @param destinationURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)convertFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:to:));

/// Converts audio using \c encoder
/// @param sourceURL The URL to convert
/// @param encoder The encoder processing the decoded audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)convertFromURL:(NSURL *)sourceURL usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:using:));

/// Converts audio from \c decoder and writes to the specified URL
/// @note The file type to create is inferred from the file extension of \c destinationURL
/// @param decoder The decoder providing the audio to convert
/// @param destinationURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder toURL:(NSURL *)destinationURL error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:to:));

/// Converts audio from \c decoder using \c encoder
/// @param decoder The decoder providing the audio to convert
/// @param encoder The encoder processing the decoded audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:using:));

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioConverter object for the given URLs or \c nil on failure
/// @param sourceURL The source URL
/// @param destinationURL The destination URL
/// @return An initialized \c SFBAudioConverter object for the specified URLs, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL NS_SWIFT_UNAVAILABLE("Use -initWithURL:destinationURL:error: instead");
/// Returns an initialized \c SFBAudioConverter object for the given URLs or \c nil on failure
/// @param sourceURL The source URL
/// @param destinationURL The destination URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioConverter object for the specified URLs, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL error:(NSError **)error;

/// Returns an initialized \c SFBAudioConverter object for the given URL and encoder or \c nil on failure
/// @param sourceURL The source URL
/// @param encoder The encoder
/// @return An initialized \c SFBAudioConverter object for the specified URL and encoder, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL encoder:(id <SFBPCMEncoding>)encoder NS_SWIFT_UNAVAILABLE("Use -initWithURL:encoder:error: instead");
/// Returns an initialized \c SFBAudioConverter object for the given URL and encoder or \c nil on failure
/// @param sourceURL The source URL
/// @param encoder The encoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioConverter object for the specified URL and encoder, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error;

/// Returns an initialized \c SFBAudioConverter object for the given decoder and URL or \c nil on failure
/// @param decoder The decoder
/// @param destinationURL The destination URL
/// @return An initialized \c SFBAudioConverter object for the specified decoder and URL, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder destinationURL:(NSURL *)destinationURL NS_SWIFT_UNAVAILABLE("Use -initWithDecoder:destinationURL:error: instead");
/// Returns an initialized \c SFBAudioConverter object for the given decoder and URL or \c nil on failure
/// @param decoder The decoder
/// @param destinationURL The destination URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioConverter object for the specified decoder and URL, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder destinationURL:(NSURL *)destinationURL error:(NSError **)error;

/// Returns an initialized \c SFBAudioConverter object for the given decoder and encoder or \c nil on failure
/// @param decoder The decoder
/// @param encoder The encoder
/// @return An initialized \c SFBAudioConverter object for the specified decoder and encoder, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder NS_SWIFT_UNAVAILABLE("Use -initWithDecoder:encoder:error: instead");
/// Returns an initialized \c SFBAudioConverter object for the given decoder and encoder or \c nil on failure
/// @param decoder The decoder
/// @param encoder The encoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioConverter object for the specified decoder and encoder, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error;

/// Returns an initialized \c SFBAudioConverter object for the given decoder and encoder or \c nil on failure
/// @param decoder The decoder
/// @param encoder The encoder
/// @param intermediateFormatBlock An optional block to receive the proposed intermediate format and return the requested intermediate format.
/// A change in intermediate format allows operations such as sample rate conversion or channel mapping.
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioConverter object for the specified decoder and encoder, or \c nil on failure
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder requestedIntermediateFormat:(AVAudioFormat *(^ _Nullable)(AVAudioFormat *))intermediateFormatBlock error:(NSError **)error NS_DESIGNATED_INITIALIZER;

#pragma mark - Conversion Information

/// The decoder supplying the audio to be converted
@property (nonatomic, readonly) id <SFBPCMDecoding> decoder;
/// The @c AVAudioConverter object producing the intermediate PCM audio
///
/// Properties such as @c channelMap, @c dither, @c downmix,
/// @c sampleRateConverterQuality, and @c sampleRateConverterAlgorithm may be set
/// before conversion.
@property (nonatomic, readonly) AVAudioConverter *intermediateConverter;
/// The encoder receving the intermediate audio for encoding
@property (nonatomic, readonly) id <SFBPCMEncoding> encoder;

#pragma mark - Conversion

/// Converts audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)convertReturningError:(NSError **)error NS_SWIFT_NAME(convert());

@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioConverter
extern NSErrorDomain const SFBAudioConverterErrorDomain NS_SWIFT_NAME(AudioConverter.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioExporter
typedef NS_ERROR_ENUM(SFBAudioConverterErrorDomain, SFBAudioConverterErrorCode) {
	/// Audio format not supported
	SFBAudioConverterErrorCodeFormatNotSupported				= 0
} NS_SWIFT_NAME(AudioConverter.ErrorCode);

NS_ASSUME_NONNULL_END
