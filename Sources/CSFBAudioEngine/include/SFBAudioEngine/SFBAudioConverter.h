//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBPCMEncoding.h>
#import <SFBAudioEngine/SFBAudioEngineErrors.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio converter converts audio from one format to another through a PCM intermediate format.
///
/// An audio converter reads PCM audio from an audio decoder in the decoder's processing format,
/// converts that audio to an intermediate PCM format, and then writes the intermediate PCM audio to an
/// audio encoder which performs the final conversion to the desired format.
///
/// The decoder's processing format and the intermediate format must both be PCM but do not have to
/// have the same sample rate, bit depth, channel count, or channel layout.
///
/// `AVAudioConverter` is used to convert from the decoder's processing format
/// to the intermediate format, performing sample rate conversion and channel mapping as required.
NS_SWIFT_NAME(AudioConverter) @interface SFBAudioConverter : NSObject

/// Converts audio and writes to the specified URL
/// - note: The file type to create is inferred from the file extension of `destinationURL`
/// - parameter sourceURL: The URL to convert
/// - parameter destinationURL: The destination URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
+ (BOOL)convertFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:to:));

/// Converts audio using `encoder`
/// - parameter sourceURL: The URL to convert
/// - parameter encoder: The encoder processing the decoded audio
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
+ (BOOL)convertFromURL:(NSURL *)sourceURL usingEncoder:(id<SFBPCMEncoding>)encoder error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:using:));

/// Converts audio from `decoder` and writes to the specified URL
/// - note: The file type to create is inferred from the file extension of `destinationURL`
/// - parameter decoder: The decoder providing the audio to convert
/// - parameter destinationURL: The destination URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
+ (BOOL)convertFromDecoder:(id<SFBPCMDecoding>)decoder toURL:(NSURL *)destinationURL error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:to:));

/// Converts audio from `decoder` using `encoder`
/// - parameter decoder: The decoder providing the audio to convert
/// - parameter encoder: The encoder processing the decoded audio
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
+ (BOOL)convertFromDecoder:(id<SFBPCMDecoding>)decoder usingEncoder:(id<SFBPCMEncoding>)encoder error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:using:));

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBAudioConverter` object for the given URLs or `nil` on failure
/// - parameter sourceURL: The source URL
/// - parameter destinationURL: The destination URL
/// - returns: An initialized `SFBAudioConverter` object for the specified URLs, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL NS_SWIFT_UNAVAILABLE("Use -initWithURL:destinationURL:error: instead");
/// Returns an initialized `SFBAudioConverter` object for the given URLs or `nil` on failure
/// - parameter sourceURL: The source URL
/// - parameter destinationURL: The destination URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioConverter` object for the specified URLs, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL error:(NSError **)error;

/// Returns an initialized `SFBAudioConverter` object for the given URL and encoder or `nil` on failure
/// - parameter sourceURL: The source URL
/// - parameter encoder: The encoder
/// - returns: An initialized `SFBAudioConverter` object for the specified URL and encoder, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL encoder:(id<SFBPCMEncoding>)encoder NS_SWIFT_UNAVAILABLE("Use -initWithURL:encoder:error: instead");
/// Returns an initialized `SFBAudioConverter` object for the given URL and encoder or `nil` on failure
/// - parameter sourceURL: The source URL
/// - parameter encoder: The encoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioConverter` object for the specified URL and encoder, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)sourceURL encoder:(id<SFBPCMEncoding>)encoder error:(NSError **)error;

/// Returns an initialized `SFBAudioConverter` object for the given decoder and URL or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter destinationURL: The destination URL
/// - returns: An initialized `SFBAudioConverter` object for the specified decoder and URL, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder destinationURL:(NSURL *)destinationURL NS_SWIFT_UNAVAILABLE("Use -initWithDecoder:destinationURL:error: instead");
/// Returns an initialized `SFBAudioConverter` object for the given decoder and URL or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter destinationURL: The destination URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioConverter` object for the specified decoder and URL, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder destinationURL:(NSURL *)destinationURL error:(NSError **)error;

/// Returns an initialized `SFBAudioConverter` object for the given decoder and encoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter encoder: The encoder
/// - returns: An initialized `SFBAudioConverter` object for the specified decoder and encoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder encoder:(id<SFBPCMEncoding>)encoder NS_SWIFT_UNAVAILABLE("Use -initWithDecoder:encoder:error: instead");
/// Returns an initialized `SFBAudioConverter` object for the given decoder and encoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter encoder: The encoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioConverter` object for the specified decoder and encoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder encoder:(id<SFBPCMEncoding>)encoder error:(NSError **)error;

/// Returns an initialized `SFBAudioConverter` object for the given decoder and encoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter encoder: The encoder
/// - parameter intermediateFormatBlock: An optional block to receive the proposed intermediate format and return the requested intermediate format.
/// A change in intermediate format allows operations such as sample rate conversion or channel mapping.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioConverter` object for the specified decoder and encoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder encoder:(id<SFBPCMEncoding>)encoder requestedIntermediateFormat:(AVAudioFormat *(^ _Nullable)(AVAudioFormat *))intermediateFormatBlock error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Closes the converter
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error;

#pragma mark - Conversion Information

/// The decoder supplying the audio to be converted
@property (nonatomic, readonly) id<SFBPCMDecoding> decoder;
/// The `AVAudioConverter` object producing the intermediate PCM audio
///
/// Properties such as `channelMap`, `dither`, `downmix`,
/// `sampleRateConverterQuality`, and `sampleRateConverterAlgorithm` may be set
/// before conversion.
@property (nonatomic, readonly) AVAudioConverter *intermediateConverter;
/// The encoder receving the intermediate audio for encoding
@property (nonatomic, readonly) id<SFBPCMEncoding> encoder;

#pragma mark - Conversion

/// Converts audio
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)convertReturningError:(NSError **)error NS_SWIFT_NAME(convert());

@end

NS_ASSUME_NONNULL_END
