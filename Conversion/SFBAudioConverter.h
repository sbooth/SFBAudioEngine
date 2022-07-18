//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBPCMEncoding.h>
#import <SFBAudioEngine/SFBAudioMetadata.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SFBAudioConverterDelegate;

/// An audio converter
NS_SWIFT_NAME(AudioConverter) @interface SFBAudioConverter : NSObject

/// Converts audio and writes to the specified URL
/// @note The file type to create is inferred from the file extension of \c destinationURL
/// @note Metadata will be read from \c sourceURL and copied to \c destinationURL
/// @param sourceURL The URL to convert
/// @param destinationURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)convertFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:to:));

/// Converts audio using \c encoder
/// @note Metadata will be read from \c sourceURL and copied to \c destinationURL
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

#pragma mark - Conversion Information

/// The decoder supplying the audio to be converted
@property (nonatomic, readonly) id <SFBPCMDecoding> decoder;
/// The encoder processing the audio
@property (nonatomic, readonly) id <SFBPCMEncoding> encoder;
/// Metadata to associate with the encoded audio
@property (nonatomic, copy, nullable) SFBAudioMetadata *metadata;

#pragma mark - Setup

/// Sets up for conversion from @c decoder to @c encoder
/// @note Not all conversion are possible.
/// @param decoder The decoder providing the source audio
/// @param encoder The desired encoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)setDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error;

#pragma mark - Conversion

/// Converts audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)convertReturningError:(NSError **)error NS_SWIFT_NAME(convert());

#pragma mark - Delegate

/// An optional delegate
@property (nonatomic, nullable, weak) id<SFBAudioConverterDelegate> delegate;

@end

#pragma mark - SFBAudioConverterDelegate

/// Delegate methods supported by \c SFBAudioConverter
NS_SWIFT_NAME(AudioConverter.Delegate) @protocol SFBAudioConverterDelegate <NSObject>
@optional
/// Called to allow the delegate to customize the processing format used for conversion
///
/// A change in processing format allows operations such as sample rate conversion or channel mapping.
/// @note The processing format must be PCM
/// @param converter The \c SFBAudioConverter object
/// @param format The proposed  PCM processing format
/// @return The desired PCM processing format
- (AVAudioFormat *)audioConverter:(SFBAudioConverter *)converter proposedProcessingFormatForConversion:(AVAudioFormat *)format;

/// Called to allow the delegate to customize the conversion parameters
/// @param converter The \c SFBAudioConverter object
/// @param audioConverter The \c AVAudioConverter object to customize
- (void)audioConverter:(SFBAudioConverter *)converter customizeConversionParameters:(AVAudioConverter *)audioConverter;
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
