/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBPCMEncoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Constant type for encoder names
typedef NSString * SFBAudioEncoderName NS_TYPED_ENUM NS_SWIFT_NAME(AudioEncoder.Name);

/// FLAC
extern SFBAudioEncoderName const SFBAudioEncoderNameFLAC;
/// Monkey's Audio
extern SFBAudioEncoderName const SFBAudioEncoderNameMonkeysAudio;
/// MP3
extern SFBAudioEncoderName const SFBAudioEncoderNameMP3;
/// Ogg FLAC
extern SFBAudioEncoderName const SFBAudioEncoderNameOggFLAC;
/// Ogg Opus
extern SFBAudioEncoderName const SFBAudioEncoderNameOggOpus;
/// Ogg Speex
extern SFBAudioEncoderName const SFBAudioEncoderNameOggSpeex;
/// Ogg Vorbis
extern SFBAudioEncoderName const SFBAudioEncoderNameOggVorbis;
/// True Audio
extern SFBAudioEncoderName const SFBAudioEncoderNameTrueAudio;
/// WavPack
extern SFBAudioEncoderName const SFBAudioEncoderNameWavPack;
/// Core Audio
extern SFBAudioEncoderName const SFBAudioEncoderNameCoreAudio;

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

- (nullable instancetype)initWithURL:(NSURL *)url encoderName:(SFBAudioEncoderName)encoderName NS_SWIFT_UNAVAILABLE("Use -initWithURL:encoderName:error: instead");
- (nullable instancetype)initWithURL:(NSURL *)url encoderName:(SFBAudioEncoderName)encoderName error:(NSError **)error;

- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBAudioEncoderName)encoderName NS_SWIFT_UNAVAILABLE("Use -initWithOutputSource:encoderName:error: instead");
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBAudioEncoderName)encoderName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

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

/// Set to nonzero to target bitrate instead of quality (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3TargetIsBitrate;
/// MP3 encoding engine algorithm quality (\c NSNumber from 0 (best) to 9 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Quality;
/// MP3 bitrate (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Bitrate;
/// Set to nonzero to encode at a constant bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3EnableCBR;
/// MP3 VBR quality (\c NSNumberfrom 0 (best) to < 10 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRQuality;
/// Set to nonzero to use fast variable bitrate mode (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3EnableFastVBR;
/// MP3 stereo mode (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3StereoMode;
/// Set to nonzero to calculate replay gain (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3CalculateReplayGain;

/// Constants for MP3 stereo modes
typedef NS_ENUM(int, SFBAudioEncoderMP3StereoMode) {
	/// Mono mode
	SFBAudioEncoderMP3StereoModeMono,
	/// Stereo mode
	SFBAudioEncoderMP3StereoModeStereo,
	/// Joint stereo mode
	SFBAudioEncoderMP3StereoModeJointStereo
} NS_SWIFT_NAME(AudioEncoder.MP3StereoMode);

/// Set to nonzero to disable resampling (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusPreserveSampleRate;
/// Ogg Opus complexity (\c NSNumber from 0 (fastest) to 10 (slowest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusComplexity;
/// Ogg Opus bitrate (\c NSNumber from 6 to 256 in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusBitrate;
/// Ogg Opus bitrate mode (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusBitrateMode;
/// Signal type (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusSignalType;
/// Frame duration in msec (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusFrameDuration;

/// Constants for Ogg Opus signal type
typedef NS_ENUM(int, SFBAudioEncoderOggOpusBitrateMode) {
	/// VBR
	SFBAudioEncoderOggOpusBitrateModeVBR,
	/// Constrained VBR
	SFBAudioEncoderOggOpusBitrateModeConstrainedVBR,
	/// Hard CBR
	SFBAudioEncoderOggOpusBitrateModeHardCBR
} NS_SWIFT_NAME(AudioEncoder.OggOpusBitrateMode);

/// Constants for Ogg Opus signal type
typedef NS_ENUM(int, SFBAudioEncoderOggOpusSignalType) {
	/// Voice
	SFBAudioEncoderOggOpusSignalTypeVoice,
	/// Music
	SFBAudioEncoderOggOpusSignalTypeMusic
} NS_SWIFT_NAME(AudioEncoder.OggOpusSignalType);

/// Constants for Ogg Opus frame duration
typedef NS_ENUM(int, SFBAudioEncoderOggOpusFrameDuration) {
	/// 2.5 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration2_5ms,
	/// 5 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration5ms,
	/// 10 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration10ms,
	/// 20 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration20ms,
	/// 40 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration40ms,
	/// 60 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration60ms,
	/// 80 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration80ms,
	/// 100 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration100ms,
	/// 120 msec
	SFBAudioEncodingSettingsKeyOggOpusFrameDuration120ms
} NS_SWIFT_NAME(AudioEncoder.OggOpusFrameDuration);

/// Ogg Speex encoding mode (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexMode;
/// Set to nonzero to target bitrate instead of quality (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexTargetIsBitrate;
/// Ogg Speex quality (\c NSNumber from 0 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexQuality;
/// Ogg Speex encoding complexity (\c NSNumber from 0 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexComplexity;
/// Ogg Speex bitrate (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexBitrate;
/// Set to nonzero to encode at a variable bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableVBR;
/// Ogg Speex VBR maximum bitrate (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexVBRMaxBitrate;
/// Set to nonzero to enable voice activity detection (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableVAD;
/// Set to nonzero to enable discontinuous transmission (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableDTX;
/// Set to nonzero to encode at an average bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableABR;
/// Set to nonzero to denoise input (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexDenoiseInput;
/// Set to nonzero to apply adaptive gain control (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableAGC;
/// Set to nonzero to disable the built-in highpass filter (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexDisableHighpassFilter;
/// The number of Speex frames per Ogg Packet (\c NSNumber from 1 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexSpeexFramesPerOggPacket;

/// Constants for Speex modes
typedef NS_ENUM(int, SFBAudioEncoderOggSpeexMode) {
	/// Narrowband
	SFBAudioEncoderOggSpeexModeNarrowband,
	/// Wideband
	SFBAudioEncoderOggSpeexModeWideband,
	/// Ultra-wideband
	SFBAudioEncoderOggSpeexModeUltraWideband
} NS_SWIFT_NAME(AudioEncoder.OggSpeexMode);

/// Set to nonzero to target bitrate instead of quality (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisTargetIsBitrate;
/// Ogg Vorbis quality (\c NSNumber from -0.1 (lowest) to 1.0 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisQuality;
/// Ogg Vorbis nominal bitrate (\c NSNumber in kpbs)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisBitrate;
/// Ogg Vorbis minimum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisMinBitrate;
/// Ogg Vorbis maximum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggVorbisMaxBitrate;

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

/// Core Audio file type ID (\c NSNumber representing \c AudioFileTypeID)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFileTypeID;
/// Core Audio format ID (\c NSNumber representing \c AudioFormatID)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatID;
/// Core Audio format flags (\c NSNumber representing \c AudioStreamBasicDescription.mFormatFlags)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatFlags;
/// Core Audio bits per channel (\c NSNumber representing \c AudioStreamBasicDescription.mBitsPerChannel)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel;
/// Core Audio audio converter configuration (\c NSDictionary keyed by \c NSNumber representing \c AudioConverterPropertyID with corresponding appropriately-wrapped value, typically \c NSNumber)
///
///	Currently supports:
///	\c kAudioConverterSampleRateConverterComplexity
///	\c kAudioConverterSampleRateConverterQuality
///	\c kAudioConverterCodecQuality
///	\c kAudioConverterEncodeBitRate
///	\c kAudioCodecPropertyBitRateControlMode
///	\c kAudioCodecPropertySoundQualityForVBR
/// \c kAudioCodecPropertyBitRateForVBR
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings;

NS_ASSUME_NONNULL_END
