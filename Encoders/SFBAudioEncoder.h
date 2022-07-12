//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

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
/// Musepack
extern SFBAudioEncoderName const SFBAudioEncoderNameMusepack;
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
/// Libsndfile
extern SFBAudioEncoderName const SFBAudioEncoderNameLibsndfile;

/// An encoder consuming PCM audio
NS_SWIFT_NAME(AudioEncoder) @interface SFBAudioEncoder : NSObject <SFBPCMEncoding>

#pragma mark - File Format Support

/// Returns a set containing the supported path extensions
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/// Returns a set containing the supported MIME types
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/// Tests whether a file extension is supported
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/// Tests whether a MIME type is supported
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

#pragma mark - Creation

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioEncoder object for the given URL or \c nil on failure
/// @param url The URL
/// @return An initialized \c SFBAudioEncoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized \c SFBAudioEncoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioEncoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized \c SFBAudioEncoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param mimeType The MIME type of \c url or \c nil
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioEncoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

/// Returns an initialized \c SFBAudioEncoder object for the given output source or \c nil on failure
/// @param outputSource The output source
/// @return An initialized \c SFBAudioEncoder object for the specified output source, or \c nil on failure
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource NS_SWIFT_UNAVAILABLE("Use -initWithOutputSource:error: instead");
/// Returns an initialized \c SFBAudioEncoder object for the given output source or \c nil on failure
/// @param outputSource The output source
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioEncoder object for the specified output source, or \c nil on failure
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource error:(NSError **)error;
/// Returns an initialized \c SFBAudioEncoder object for the given output source or \c nil on failure
/// @param outputSource The output source
/// @param mimeType The MIME type of \c outputSource or \c nil
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioEncoder object for the specified output source, or \c nil on failure
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized \c SFBAudioEncoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param encoderName The name of the encoder to use
/// @return An initialized \c SFBAudioEncoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url encoderName:(SFBAudioEncoderName)encoderName NS_SWIFT_UNAVAILABLE("Use -initWithURL:encoderName:error: instead");
/// Returns an initialized \c SFBAudioEncoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param encoderName The name of the encoder to use
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioEncoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url encoderName:(SFBAudioEncoderName)encoderName error:(NSError **)error;

/// Returns an initialized \c SFBAudioEncoder object for the given output source or \c nil on failure
/// @param outputSource The output source
/// @param encoderName The name of the encoder to use
/// @return An initialized \c SFBAudioEncoder object for the specified output source, or \c nil on failure
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBAudioEncoderName)encoderName NS_SWIFT_UNAVAILABLE("Use -initWithOutputSource:encoderName:error: instead");
/// Returns an initialized \c SFBAudioEncoder object for the given output source or \c nil on failure
/// @param outputSource The output source
/// @param encoderName The name of the encoder to use
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioEncoder object for the specified output source, or \c nil on failure
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBAudioEncoderName)encoderName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Opens the encoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
/// Closes the encoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioEncoder and subclasses
extern NSErrorDomain const SFBAudioEncoderErrorDomain NS_SWIFT_NAME(AudioEncoder.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioEncoder
typedef NS_ERROR_ENUM(SFBAudioEncoderErrorDomain, SFBAudioEncoderErrorCode) {
	/// Internal or unspecified encoder error
	SFBAudioEncoderErrorCodeInternalError	= 0,
	/// Unknown encoder name
	SFBAudioEncoderErrorCodeUnknownEncoder	= 1,
	/// Invalid, unknown, or unsupported format
	SFBAudioEncoderErrorCodeInvalidFormat	= 2
} NS_SWIFT_NAME(AudioEncoder.ErrorCode);

#pragma mark - FLAC Encoder Settings

/// FLAC compression level (\c NSNumber from 1 (lowest) to 8 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACCompressionLevel;
/// Set to nonzero to verify FLAC encoding (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACVerifyEncoding;

#pragma mark - Monkey's Audio Encoder Settings

/// APE compression level (\c SFBAudioEncodingSettingsValueAPECompressionLevel)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyAPECompressionLevel;

/// Constant type for APE compression levels
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueAPECompressionLevel NS_TYPED_ENUM NS_SWIFT_NAME(APECompressionLevel);

/// Fast compression
extern SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelFast;
/// Normal compression
extern SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelNormal;
/// High compression
extern SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelHigh;
/// Extra high compression
extern SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelExtraHigh;
/// Insane compression
extern SFBAudioEncodingSettingsValueAPECompressionLevel const SFBAudioEncodingSettingsValueAPECompressionLevelInsane;

#pragma mark - MP3 Encoder Settings

// Valid bitrates for MPEG 1 Layer III are 32 40 48 56 64 80 96 112 128 160 192 224 256 320

/// MP3 encoding engine algorithm quality (\c NSNumber from 0 (best) to 9 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Quality;
/// Bitrate for CBR encoding (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3ConstantBitrate;
/// Target bitrate for ABR encoding (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3AverageBitrate;
/// Set to nonzero for VBR encoding (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3UseVariableBitrate;
/// Quality setting for VBR encoding (\c NSNumberfrom 0 (best) to < 10 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRQuality;
/// Minimum bitrate for VBR encoding (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRMinimumBitrate;
/// Maximum bitrate for VBR encoding (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRMaximumBitrate;
/// MP3 stereo mode (\c SFBAudioEncodingSettingsValueMP3StereoMode)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3StereoMode;
/// Set to nonzero to calculate replay gain (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3CalculateReplayGain;

/// Constant type for MP3 stereo modes
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueMP3StereoMode NS_TYPED_ENUM NS_SWIFT_NAME(MP3StereoMode);

/// Mono mode
extern SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeMono;
/// Stereo mode
extern SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeStereo;
/// Joint stereo mode
extern SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeJointStereo;

#pragma mark - Musepack Encoder Settings

/// Musepack quality (\c NSNumber from 0.0 (worst) to 10.0 (best))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMusepackQuality;

#pragma mark - Opus Encoder Settings

/// Set to nonzero to disable resampling (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusPreserveSampleRate;
/// Opus complexity (\c NSNumber from 0 (fastest) to 10 (slowest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusComplexity;
/// Opus bitrate (\c NSNumber from 6 to 256 in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrate;
/// Opus bitrate mode (\c SFBAudioEncodingSettingsValueOpusBitrateMode)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrateMode;
/// Opus signal type (\c SFBAudioEncodingSettingsValueOpusSignalType)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusSignalType;
/// Opus frame duration (\c SFBAudioEncodingSettingsValueOpusFrameDuration)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusFrameDuration;

/// Constant type for Opus bitrate modes
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueOpusBitrateMode NS_TYPED_ENUM NS_SWIFT_NAME(OpusBitrateMode);

/// VBR
extern SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeVBR;
/// Constrained VBR
extern SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeConstrainedVBR;
/// Hard CBR
extern SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeHardCBR;

/// Constant type for Opus signal type
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueOpusSignalType NS_TYPED_ENUM NS_SWIFT_NAME(OpusSignalType);

/// Voice
extern SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeVoice;
/// Music
extern SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeMusic;

/// Constant type for Opus frame duration
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueOpusFrameDuration NS_TYPED_ENUM NS_SWIFT_NAME(OpusFrameDuration);

/// 2.5 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration2_5ms;
/// 5 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration5ms;
/// 10 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration10ms;
/// 20 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration20ms;
/// 40 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration40ms;
/// 60 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration60ms;
/// 80 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration80ms;
/// 100 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration100ms;
/// 120 msec
extern SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration120ms;

#pragma mark - Speex Encoder Settings

/// Speex encoding mode (\c SFBAudioEncodingSettingsValueSpeexMode)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexMode;
/// Set to nonzero to target bitrate instead of quality (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexTargetIsBitrate;
/// Speex quality (\c NSNumber from 0 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexQuality;
/// Speex encoding complexity (\c NSNumber from 0 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexComplexity;
/// Speex bitrate (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexBitrate;
/// Set to nonzero to encode at a variable bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableVBR;
/// Speex VBR maximum bitrate (\c NSNumber in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexVBRMaxBitrate;
/// Set to nonzero to enable voice activity detection (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableVAD;
/// Set to nonzero to enable discontinuous transmission (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableDTX;
/// Set to nonzero to encode at an average bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableABR;
/// Set to nonzero to denoise input (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexDenoiseInput;
/// Set to nonzero to apply adaptive gain control (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableAGC;
/// Set to nonzero to disable the built-in highpass filter (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexDisableHighpassFilter;
/// The number of Speex frames per Ogg Packet (\c NSNumber from 1 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexFramesPerOggPacket;

/// Constant type for Speex modes
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueSpeexMode NS_TYPED_ENUM NS_SWIFT_NAME(SpeexMode);

/// Narrowband
extern SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeNarrowband;
/// Wideband
extern SFBAudioEncodingSettingsValueSpeexMode  const SFBAudioEncodingSettingsValueSpeexModeWideband;
/// Ultra-wideband
extern SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeUltraWideband;

#pragma mark - Vorbis Encoder Settings

/// Set to nonzero to target bitrate instead of quality (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisTargetIsBitrate;
/// Vorbis quality (\c NSNumber from -0.1 (lowest) to 1.0 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisQuality;
/// Vorbis nominal bitrate (\c NSNumber in kpbs)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisBitrate;
/// Vorbis minimum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisMinBitrate;
/// Vorbis maximum bitrate (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisMaxBitrate;

#pragma mark - WavPack Encoder Settings

/// WavPack compression level (\c SFBAudioEncodingSettingsValueWavPackCompressionLevel)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyWavPackCompressionLevel;

/// Constant type for WavPack  compression levels
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueWavPackCompressionLevel NS_TYPED_ENUM NS_SWIFT_NAME(WavPackCompressionLevel);

/// Fast compression
extern SFBAudioEncodingSettingsValueWavPackCompressionLevel const SFBAudioEncodingSettingsValueWavPackCompressionLevelFast;
/// High compression
extern SFBAudioEncodingSettingsValueWavPackCompressionLevel const SFBAudioEncodingSettingsValueWavPackCompressionLevelHigh;
/// Very high ompression
extern SFBAudioEncodingSettingsValueWavPackCompressionLevel const SFBAudioEncodingSettingsValueWavPackCompressionLevelVeryHigh;

#pragma mark - Core Audio Encoder Settings

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
/// Currently supports:
/// \c kAudioConverterSampleRateConverterComplexity
/// \c kAudioConverterSampleRateConverterQuality
/// \c kAudioConverterCodecQuality
/// \c kAudioConverterEncodeBitRate
/// \c kAudioCodecPropertyBitRateControlMode
/// \c kAudioCodecPropertySoundQualityForVBR
/// \c kAudioCodecPropertyBitRateForVBR
/// \c kAudioConverterPropertyDithering (macOS only)
/// \c kAudioConverterPropertyDitherBitDepth (macOS only)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings;

#pragma mark - Libsndfile Encoder Settings

/// Libsndfile major format (\c SFBAudioEncodingSettingsValueLibsndfileMajorFormat)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileMajorFormat;
/// Libsndfile subtype (\c SFBAudioEncodingSettingsValueLibsndfileSubtype)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileSubtype;
/// Libsndfile output file endian-ness (\c SFBAudioEncodingSettingsValueLibsndfileFileEndian)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileFileEndian;

/// Constant type for Libsndfile major formats
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueLibsndfileMajorFormat NS_TYPED_ENUM NS_SWIFT_NAME(LibsndfileMajorFormat);

/// Microsoft WAV format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatWAV;
/// Apple/SGI AIFF format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatAIFF;
/// Sun/NeXT AU format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatAU;
/// \b RAW PCM data
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatRaw;
/// Ensoniq PARIS file format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatPAF;
/// Amiga IFF / SVX8 / SV16 format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatSVX;
/// Sphere NIST format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatNIST;
/// VOC files
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatVOC;
/// Berkeley/IRCAM/CARL
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatIRCAM;
/// Sonic Foundry's 64 bit RIFF/WAV
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatW64;
/// Matlab (tm) V4.2 / GNU Octave 2.0
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatMAT4;
/// Matlab (tm) V5.0 / GNU Octave 2.1
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatMAT5;
/// Portable Voice Format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatPVF;
/// Fasttracker 2 Extended Instrument
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatXI;
/// HMM Tool Kit format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatHTK;
/// Midi Sample Dump Standard
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatSDS;
/// Audio Visual Research
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatAVR;
/// MS WAVE with WAVEFORMATEX
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatWAVEX;
/// Sound Designer 2
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatSD2;
/// FLAC lossless file format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatFLAC;
/// Core Audio File format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatCAF;
/// Psion WVE format
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatWVE;
/// Xiph OGG container
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatOgg;
/// Akai MPC 2000 sampler
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatMPC2K;
/// RF64 WAV file
extern SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatRF64;

/// Constant type for Libsndfile subtypes
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueLibsndfileSubtype NS_TYPED_ENUM NS_SWIFT_NAME(LibsndfileSubtype);

/// Signed 8 bit data
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_S8;
/// Signed 16 bit data
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_16;
/// Signed 24 bit data
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_24;
/// Signed 32 bit data
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_32;
/// Unsigned 8 bit data (WAV and RAW only)
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_U8;
/// 32 bit float data
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeFloat;
/// 64 bit float data
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDouble;
/// U-Law encoded
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeµLAW;
/// A-Law encoded
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAW;
/// IMA ADPCM
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeIMA_ADPCM;
/// Microsoft ADPCM
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeMS_ADPCM;
/// GSM 6.10 encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeGSM610;
/// OKI / Dialogix ADPCM
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeVOX_ADPCM;
/// 16kbs NMS G721-variant encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_16;
/// 24kbs NMS G721-variant encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_24;
/// 32kbs NMS G721-variant encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_32;
/// 32kbs G721 ADPCM encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeG721_32;
/// 24kbs G723 ADPCM encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeG723_24;
/// 40kbs G723 ADPCM encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeG723_40;
/// 12 bit Delta Width Variable Word encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_12;
/// 16 bit Delta Width Variable Word encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_16;
/// 24 bit Delta Width Variable Word encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_24;
/// N bit Delta Width Variable Word encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_N;
/// 8 bit differential PCM (XI only)
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDPCM_8;
/// 16 bit differential PCM (XI only)
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDPCM_16;
/// Xiph Vorbis encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeVorbis;
/// Xiph/Skype Opus encoding
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeOpus;
/// Apple Lossless Audio Codec (16 bit)
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_16;
/// Apple Lossless Audio Codec (20 bit)
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_20;
/// Apple Lossless Audio Codec (24 bit)
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_24;
/// Apple Lossless Audio Codec (32 bit)
extern SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_32;

/// Constant type for Libsndfile file endian-ness
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueLibsndfileFileEndian NS_TYPED_ENUM NS_SWIFT_NAME(LibsndfileFileEndian);

/// Default file endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianDefault;
/// Force little endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianLittle;
/// Force big endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianBig;
/// Force CPU endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianCPU;

NS_ASSUME_NONNULL_END
