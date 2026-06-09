//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <SFBAudioEngine/SFBPCMEncoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Constant type for encoder names
typedef NSString *SFBAudioEncoderName NS_TYPED_ENUM NS_SWIFT_NAME(AudioEncoder.Name);

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
NS_SWIFT_NAME(AudioEncoder)
@interface SFBAudioEncoder : NSObject <SFBPCMEncoding>

// MARK: - File Format Support

/// A set containing the supported path extensions
@property(class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/// Returns a set containing the supported MIME types
@property(class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/// Tests whether a file extension is supported
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/// Tests whether a MIME type is supported
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

// MARK: - Creation

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBAudioEncoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - returns: An initialized `SFBAudioEncoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized `SFBAudioEncoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioEncoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBAudioEncoder` object for the given URL or `nil` on failure
/// - important: If there is a conflict between the URL's path extension and the MIME type, the MIME type takes
/// precedence
/// - parameter url: The URL
/// - parameter mimeType: The MIME type of `url` or `nil`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioEncoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

/// Returns an initialized `SFBAudioEncoder` object for the given output target or `nil` on failure
/// - parameter outputTarget: The output target
/// - returns: An initialized `SFBAudioEncoder` object for the specified output target, or `nil` on failure
- (nullable instancetype)initWithOutputTarget:(SFBOutputTarget *)outputTarget
        NS_SWIFT_UNAVAILABLE("Use -initWithOutputTarget:error: instead");
/// Returns an initialized `SFBAudioEncoder` object for the given output target or `nil` on failure
/// - parameter outputTarget: The output target
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioEncoder` object for the specified output target, or `nil` on failure
- (nullable instancetype)initWithOutputTarget:(SFBOutputTarget *)outputTarget error:(NSError **)error;
/// Returns an initialized `SFBAudioEncoder` object for the given output target or `nil` on failure
/// - parameter outputTarget: The output target
/// - parameter mimeType: The MIME type of `outputTarget` or `nil`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioEncoder` object for the specified output target, or `nil` on failure
- (nullable instancetype)initWithOutputTarget:(SFBOutputTarget *)outputTarget
                                     mimeType:(nullable NSString *)mimeType
                                        error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized `SFBAudioEncoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter encoderName: The name of the encoder to use
/// - returns: An initialized `SFBAudioEncoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url
                         encoderName:(SFBAudioEncoderName)encoderName
        NS_SWIFT_UNAVAILABLE("Use -initWithURL:encoderName:error: instead");
/// Returns an initialized `SFBAudioEncoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter encoderName: The name of the encoder to use
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioEncoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url encoderName:(SFBAudioEncoderName)encoderName error:(NSError **)error;

/// Returns an initialized `SFBAudioEncoder` object for the given output target or `nil` on failure
/// - parameter outputTarget: The output target
/// - parameter encoderName: The name of the encoder to use
/// - returns: An initialized `SFBAudioEncoder` object for the specified output target, or `nil` on failure
- (nullable instancetype)initWithOutputTarget:(SFBOutputTarget *)outputTarget
                                  encoderName:(SFBAudioEncoderName)encoderName
        NS_SWIFT_UNAVAILABLE("Use -initWithOutputTarget:encoderName:error: instead");
/// Returns an initialized `SFBAudioEncoder` object for the given output target or `nil` on failure
/// - parameter outputTarget: The output target
/// - parameter encoderName: The name of the encoder to use
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioEncoder` object for the specified output target, or `nil` on failure
- (nullable instancetype)initWithOutputTarget:(SFBOutputTarget *)outputTarget
                                  encoderName:(SFBAudioEncoderName)encoderName
                                        error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Opens the encoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
/// Closes the encoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

// MARK: - Error Information

/// The `NSErrorDomain` used by `SFBAudioEncoder` and subclasses
extern NSErrorDomain const SFBAudioEncoderErrorDomain NS_SWIFT_NAME(AudioEncoder.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioEncoder`
typedef NS_ERROR_ENUM(SFBAudioEncoderErrorDomain, SFBAudioEncoderErrorCode){
    /// Unknown encoder name
    SFBAudioEncoderErrorCodeUnknownEncoder = 0,
    /// Invalid, unknown, or unsupported format
    SFBAudioEncoderErrorCodeInvalidFormat = 1,
    /// Internal or unspecified encoder error
    SFBAudioEncoderErrorCodeInternalError = 2,
} NS_SWIFT_NAME(AudioEncoder.Error);

// MARK: - FLAC Encoder Settings

/// FLAC compression level (`NSNumber` from 0 (lowest) to 8 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACCompressionLevel;
/// Set to nonzero to verify FLAC encoding (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACVerifyEncoding;

// MARK: - Monkey's Audio Encoder Settings

/// APE compression level (`SFBAudioEncodingSettingsValueAPECompressionLevel`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyAPECompressionLevel;

/// Constant type for APE compression levels
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueAPECompressionLevel
        NS_TYPED_ENUM NS_SWIFT_NAME(APECompressionLevel);

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

// MARK: - MP3 Encoder Settings

// Valid bitrates for MPEG 1 Layer III are 32 40 48 56 64 80 96 112 128 160 192 224 256 320

/// MP3 encoding engine algorithm quality (`NSNumber` from 0 (best) to 9 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Quality;
/// Bitrate for CBR encoding (`NSNumber` in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3ConstantBitrate;
/// Target bitrate for ABR encoding (`NSNumber` in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3AverageBitrate;
/// Set to nonzero for VBR encoding (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3UseVariableBitrate;
/// Quality setting for VBR encoding (`NSNumberfrom` 0 (best) to < 10 (worst))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRQuality;
/// Minimum bitrate for VBR encoding (`NSNumber` in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRMinimumBitrate;
/// Maximum bitrate for VBR encoding (`NSNumber` in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRMaximumBitrate;
/// MP3 stereo mode (`SFBAudioEncodingSettingsValueMP3StereoMode`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3StereoMode;
/// Set to nonzero to calculate replay gain (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3CalculateReplayGain;

/// Constant type for MP3 stereo modes
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueMP3StereoMode
        NS_TYPED_ENUM NS_SWIFT_NAME(MP3StereoMode);

/// Mono mode
extern SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeMono;
/// Stereo mode
extern SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeStereo;
/// Joint stereo mode
extern SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeJointStereo;

// MARK: - Musepack Encoder Settings

/// Musepack quality (`NSNumber` from 0.0 (worst) to 10.0 (best))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMusepackQuality;

// MARK: - Opus Encoder Settings

/// Set to nonzero to disable resampling (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusPreserveSampleRate;
/// Opus complexity (`NSNumber` from 0 (fastest) to 10 (slowest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusComplexity;
/// Opus bitrate (`NSNumber` from 6 to 256 in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrate;
/// Opus bitrate mode (`SFBAudioEncodingSettingsValueOpusBitrateMode`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrateMode;
/// Opus signal type (`SFBAudioEncodingSettingsValueOpusSignalType`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusSignalType;
/// Opus frame duration (`SFBAudioEncodingSettingsValueOpusFrameDuration`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusFrameDuration;

/// Constant type for Opus bitrate modes
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueOpusBitrateMode
        NS_TYPED_ENUM NS_SWIFT_NAME(OpusBitrateMode);

/// VBR
extern SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeVBR;
/// Constrained VBR
extern SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeConstrainedVBR;
/// Hard CBR
extern SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeHardCBR;

/// Constant type for Opus signal type
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueOpusSignalType
        NS_TYPED_ENUM NS_SWIFT_NAME(OpusSignalType);

/// Voice
extern SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeVoice;
/// Music
extern SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeMusic;

/// Constant type for Opus frame duration
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueOpusFrameDuration
        NS_TYPED_ENUM NS_SWIFT_NAME(OpusFrameDuration);

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

// MARK: - Speex Encoder Settings

/// Speex encoding mode (`SFBAudioEncodingSettingsValueSpeexMode`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexMode;
/// Set to nonzero to target bitrate instead of quality (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexTargetIsBitrate;
/// Speex quality (`NSNumber` from 0 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexQuality;
/// Speex encoding complexity (`NSNumber` from 0 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexComplexity;
/// Speex bitrate (`NSNumber` in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexBitrate;
/// Set to nonzero to encode at a variable bitrate (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableVBR;
/// Speex VBR maximum bitrate (`NSNumber` in kbps)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexVBRMaxBitrate;
/// Set to nonzero to enable voice activity detection (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableVAD;
/// Set to nonzero to enable discontinuous transmission (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableDTX;
/// Set to nonzero to encode at an average bitrate (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableABR;
/// Set to nonzero to denoise input (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexDenoiseInput;
/// Set to nonzero to apply adaptive gain control (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexEnableAGC;
/// Set to nonzero to disable the built-in highpass filter (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexDisableHighpassFilter;
/// The number of Speex frames per Ogg Packet (`NSNumber` from 1 to 10)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeySpeexFramesPerOggPacket;

/// Constant type for Speex modes
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueSpeexMode NS_TYPED_ENUM NS_SWIFT_NAME(SpeexMode);

/// Narrowband
extern SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeNarrowband;
/// Wideband
extern SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeWideband;
/// Ultra-wideband
extern SFBAudioEncodingSettingsValueSpeexMode const SFBAudioEncodingSettingsValueSpeexModeUltraWideband;

// MARK: - Vorbis Encoder Settings

/// Set to nonzero to target bitrate instead of quality (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisTargetIsBitrate;
/// Vorbis quality (`NSNumber` from -0.1 (lowest) to 1.0 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisQuality;
/// Vorbis nominal bitrate (`NSNumber` in kpbs)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisBitrate;
/// Vorbis minimum bitrate (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisMinBitrate;
/// Vorbis maximum bitrate (`NSNumber`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyVorbisMaxBitrate;

// MARK: - WavPack Encoder Settings

/// WavPack compression level (`SFBAudioEncodingSettingsValueWavPackCompressionLevel`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyWavPackCompressionLevel;

/// Constant type for WavPack  compression levels
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueWavPackCompressionLevel
        NS_TYPED_ENUM NS_SWIFT_NAME(WavPackCompressionLevel);

/// Fast compression
extern SFBAudioEncodingSettingsValueWavPackCompressionLevel const
        SFBAudioEncodingSettingsValueWavPackCompressionLevelFast;
/// High compression
extern SFBAudioEncodingSettingsValueWavPackCompressionLevel const
        SFBAudioEncodingSettingsValueWavPackCompressionLevelHigh;
/// Very high ompression
extern SFBAudioEncodingSettingsValueWavPackCompressionLevel const
        SFBAudioEncodingSettingsValueWavPackCompressionLevelVeryHigh;

// MARK: - Core Audio Encoder Settings

/// Core Audio file type ID (`NSNumber` representing `AudioFileTypeID`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFileTypeID;
/// Core Audio format ID (`NSNumber` representing `AudioFormatID`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatID;
/// Core Audio format flags (`NSNumber` representing `AudioStreamBasicDescription`.mFormatFlags)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatFlags;
/// Core Audio bits per channel (`NSNumber` representing `AudioStreamBasicDescription`.mBitsPerChannel)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel;
/// Core Audio audio converter configuration (`NSDictionary` keyed by `NSNumber` representing `AudioConverterPropertyID`
/// with corresponding appropriately-wrapped value, typically `NSNumber`)
///
/// Currently supports:
/// `kAudioConverterSampleRateConverterComplexity`
/// `kAudioConverterSampleRateConverterQuality`
/// `kAudioConverterCodecQuality`
/// `kAudioConverterEncodeBitRate`
/// `kAudioCodecPropertyBitRateControlMode`
/// `kAudioCodecPropertySoundQualityForVBR`
/// `kAudioCodecPropertyBitRateForVBR`
/// `kAudioConverterPropertyDithering` (macOS only)
/// `kAudioConverterPropertyDitherBitDepth` (macOS only)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings;

// MARK: - Libsndfile Encoder Settings

/// Libsndfile major format (`SFBAudioEncodingSettingsValueLibsndfileMajorFormat`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileMajorFormat;
/// Libsndfile subtype (`SFBAudioEncodingSettingsValueLibsndfileSubtype`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileSubtype;
/// Libsndfile output file endian-ness (`SFBAudioEncodingSettingsValueLibsndfileFileEndian`)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileFileEndian;

/// Constant type for Libsndfile major formats
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueLibsndfileMajorFormat
        NS_TYPED_ENUM NS_SWIFT_NAME(LibsndfileMajorFormat);

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
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueLibsndfileSubtype
        NS_TYPED_ENUM NS_SWIFT_NAME(LibsndfileSubtype);

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
typedef SFBAudioEncodingSettingsValue SFBAudioEncodingSettingsValueLibsndfileFileEndian
        NS_TYPED_ENUM NS_SWIFT_NAME(LibsndfileFileEndian);

/// Default file endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianDefault;
/// Force little endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianLittle;
/// Force big endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianBig;
/// Force CPU endian-ness
extern SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianCPU;

NS_ASSUME_NONNULL_END
