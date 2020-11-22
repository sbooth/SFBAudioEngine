/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBPCMEncoding.h>

NS_ASSUME_NONNULL_BEGIN

/// An encoder consuming PCM audio
NS_SWIFT_NAME(AudioEncoder) @interface SFBAudioEncoder : NSObject <SFBPCMEncoding>

#pragma mark - File Format Support

/// Returns a set containing the supported path extensions
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/*!@brief Returns a set containing the supported MIME types */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/// Tests whether a file extension is supported
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/// Tests whether a MIME type is supported
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

#pragma mark - Creation

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource NS_SWIFT_UNAVAILABLE("Use -initWithOutputSource:error: instead");
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource error:(NSError **)error;
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioEncoder and subclasses
extern NSErrorDomain const SFBAudioEncoderErrorDomain NS_SWIFT_NAME(AudioEncoder.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioEncoder
typedef NS_ERROR_ENUM(SFBAudioEncoderErrorDomain, SFBAudioEncoderErrorCode) {
	/// File not found
	SFBAudioEncoderErrorCodeFileNotFound	= 0,
	/// Input/output error
	SFBAudioEncoderErrorCodeInputOutput		= 1,
	/// Invalid desired format
	SFBAudioEncoderErrorCodeInvalidFormat	= 2,
	/// Internal encoder error
	SFBAudioEncoderErrorCodeInternalError	= 3
} NS_SWIFT_NAME(AudioEncoder.ErrorCode);

#pragma mark - Encoder Settings

// Encoder settings dictionary keys
/// FLAC compression level (\c NSNumber from 1 (lowest) to 8 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACCompressionLevel;
/// Set to nonzero to verify FLAC encoding (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACVerifyEncoding;
/// APE compression level (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyAPECompressionLevel;
/// WavPack compression level (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyWavPackCompressionLevel;
/// Ogg Vorbis mode (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisMode;
/// Ogg Vorbis bitrate (\c NSNumber from -0.1 (lowest) to 1.0 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisQuality;
/// Ogg Vorbis nominal bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisBitrate;
/// Ogg Vorbis minimum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisMinBitrate;
/// Ogg Vorbis maximum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisMaxBitrate;

/// Constants for APE compression levels
typedef NS_ENUM(int, SFBAudioEncoderAPECompressionLevel) {
	/// Fast compression
	SFBAudioEncoderAPECompressionLevelFast,
	/// Normal compression
	SFBAudioEncoderAPECompressionLevelNormal,
	/// High compression
	SFBAudioEncoderAPECompressionLevelHigh,
	/// Extra high compression
	SFBAudioEncoderAPECompressionLevelExtraHigh,
	/// Insane compression
	SFBAudioEncoderAPECompressionLevelInsane
} NS_SWIFT_NAME(AudioEncoder.APECompressionLevel);

/// Constants for WavPack  compression levels
typedef NS_ENUM(int, SFBAudioEncoderWavPackCompressionLevel) {
	/// Fast compression
	SFBAudioEncoderWavPackCompressionLevelFast,
	/// High compression
	SFBAudioEncoderWavPackCompressionLevelHigh,
	/// Very high ompression
	SFBAudioEncoderWavPackCompressionLevelVeryHigh
} NS_SWIFT_NAME(AudioEncoder.WavPackCompressionLevel);

/// Constants for Ogg Vorbis encoding modes
typedef NS_ENUM(int, SFBAudioEncoderOggVorbisMode) {
	/// Quality mode
	SFBAudioEncoderOggVorbisModeQuality,
	/// Bitrate mode
	SFBAudioEncoderOggVorbisModeBitrate
} NS_SWIFT_NAME(AudioEncoder.OggVorbisMode);

NS_ASSUME_NONNULL_END
