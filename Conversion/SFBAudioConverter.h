/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBPCMEncoding.h>
#import <SFBAudioEngine/SFBAudioMetadata.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio converter
NS_SWIFT_NAME(AudioConverter) @interface SFBAudioConverter : NSObject

/// Converts audio and writes to the specified URL
/// @note The file type to create is inferred from the file extension of \c destinationURL
/// @param sourceURL The URL to convert
/// @param destinationURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)convertURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:to:));

/// Converts audio using the specified encoder
/// @param decoder The decoder providing the audio to convert
/// @param encoder The encoder processing the decoded audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)convertAudioFromDecoder:(id <SFBPCMDecoding>)decoder usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error NS_SWIFT_NAME(AudioConverter.convert(_:using:));;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL NS_SWIFT_UNAVAILABLE("Use -initWithURL:destinationURL:error: instead");
- (nullable instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL error:(NSError **)error;

- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder NS_SWIFT_UNAVAILABLE("Use -initWithDecoder:encoder:error: instead");
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error;
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder metadata:(nullable SFBAudioMetadata *)metadata NS_SWIFT_UNAVAILABLE("Use -initWithDecoder:encoder:metadata:error: instead");
- (nullable instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder metadata:(nullable SFBAudioMetadata *)metadata error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// The decoder supplying the audio to be converted
@property (nonatomic, readonly) id <SFBPCMDecoding> decoder;
/// The encoder processing the audio
@property (nonatomic, readonly) id <SFBPCMEncoding> encoder;
/// Metadata to associate with the encoded audio
@property (nonatomic, nullable) SFBAudioMetadata *metadata;

/// Converts audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)convertReturningError:(NSError **)error NS_SWIFT_NAME(convert());

@end

/// The \c NSErrorDomain used by \c SFBAudioConverter
extern NSErrorDomain const SFBAudioConverterErrorDomain NS_SWIFT_NAME(AudioConverter.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioExporter
typedef NS_ERROR_ENUM(SFBAudioConverterErrorDomain, SFBAudioConverterErrorCode) {
	/// Audio format not supported
	SFBAudioConverterErrorCodeFormatNotSupported				= 0
} NS_SWIFT_NAME(AudioConverter.ErrorCode);

NS_ASSUME_NONNULL_END
