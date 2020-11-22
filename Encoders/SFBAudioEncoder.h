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

/// WavPack compression level (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyWavPackCompressionLevel;

/// Constants for WavPack  compression levels
typedef NS_ENUM(int, SFBAudioEncoderWavPackCompressionLevel) {
	/// Fast compression
	SFBAudioEncoderWavPackCompressionLevelFast,
	/// High compression
	SFBAudioEncoderWavPackCompressionLevelHigh,
	/// Very high ompression
	SFBAudioEncoderWavPackCompressionLevelVeryHigh
} NS_SWIFT_NAME(AudioEncoder.WavPackCompressionLevel);

/// Ogg Vorbis encoding target (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisEncodingTarget;
/// Ogg Vorbis quality (\c NSNumber from -0.1 (lowest) to 1.0 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisQuality;
/// Ogg Vorbis nominal bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisBitrate;
/// Ogg Vorbis minimum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisMinBitrate;
/// Ogg Vorbis maximum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisMaxBitrate;

/// Constants for Ogg Vorbis encoding targets
typedef NS_ENUM(int, SFBAudioEncoderOggVorbisEncodingTarget) {
	/// Quality mode
	SFBAudioEncoderOggVorbisEncodingTargetQuality,
	/// Bitrate mode
	SFBAudioEncoderOggVorbisEncodingTargetBitrate
} NS_SWIFT_NAME(AudioEncoder.OggVorbisEncodingTarget);

/// MP3 encoding target (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3EncodingTarget;
/// MP3 encoding engine algorithm quality (\c NSNumber from 0 (best) to 9 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Quality;
/// MP3 bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Bitrate;
/// Set to nonzero to encode at a constant bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3CBR;
/// Set to nonzero to use fast variable bitrate mode (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3FastVBR;
/// MP3 VBR quality (\c NSNumber from 0 (best) to < 10 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRQuality;
/// MP3 stereo mode (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3StereoMode;
/// Set to nonzero to calculate replay gain (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3CalculateReplayGain;

/// Constants for MP3 encoding targets
typedef NS_ENUM(int, SFBAudioEncoderMP3EncodingTarget) {
	/// Quality mode
	SFBAudioEncoderMP3EncodingTargetQuality,
	/// Bitrate mode
	SFBAudioEncoderMP3EncodingTargetBitrate
} NS_SWIFT_NAME(AudioEncoder.MP3EncodingTarget);

/// Constants for MP3 stereo modes
typedef NS_ENUM(int, SFBAudioEncoderMP3StereoMode) {
	/// Mono mode
	SFBAudioEncoderMP3StereoModeMono,
	/// Stereo mode
	SFBAudioEncoderMP3StereoModeStereo,
	/// Joint stereo mode
	SFBAudioEncoderMP3StereoModeJointStereo
} NS_SWIFT_NAME(AudioEncoder.MP3StereoMode);

NS_ASSUME_NONNULL_END
