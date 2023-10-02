//
// Copyright (c) 2006 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBPCMDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Constant type for decoder names
typedef NSString * SFBAudioDecoderName NS_TYPED_ENUM NS_SWIFT_NAME(AudioDecoder.Name);

/// FLAC and Ogg FLAC
extern SFBAudioDecoderName const SFBAudioDecoderNameFLAC;
/// Monkey's Audio
extern SFBAudioDecoderName const SFBAudioDecoderNameMonkeysAudio;
/// Module
extern SFBAudioDecoderName const SFBAudioDecoderNameModule;
/// MPEG 1/2/2.5 Layers I, II, and III
extern SFBAudioDecoderName const SFBAudioDecoderNameMPEG;
/// Musepack
extern SFBAudioDecoderName const SFBAudioDecoderNameMusepack;
/// Ogg Opus
extern SFBAudioDecoderName const SFBAudioDecoderNameOggOpus;
/// Ogg Speex
extern SFBAudioDecoderName const SFBAudioDecoderNameOggSpeex;
/// Ogg Vorbis
extern SFBAudioDecoderName const SFBAudioDecoderNameOggVorbis;
/// Shorten
extern SFBAudioDecoderName const SFBAudioDecoderNameShorten;
/// True Audio
extern SFBAudioDecoderName const SFBAudioDecoderNameTrueAudio;
/// WavPack
extern SFBAudioDecoderName const SFBAudioDecoderNameWavPack;
/// Core Audio
extern SFBAudioDecoderName const SFBAudioDecoderNameCoreAudio;
/// Libsndfile
extern SFBAudioDecoderName const SFBAudioDecoderNameLibsndfile;

/// A decoder providing audio as PCM
NS_SWIFT_NAME(AudioDecoder) @interface SFBAudioDecoder : NSObject <SFBPCMDecoding>

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

/// Returns an initialized \c SFBAudioDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @return An initialized \c SFBAudioDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized \c SFBAudioDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized \c SFBAudioDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param mimeType The MIME type of \c url or \c nil
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

/// Returns an initialized \c SFBAudioDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @return An initialized \c SFBAudioDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:error: instead");
/// Returns an initialized \c SFBAudioDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized \c SFBAudioDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param mimeType The MIME type of \c inputSource or \c nil
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized \c SFBAudioDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param decoderName The name of the decoder to use
/// @return An initialized \c SFBAudioDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithURL:decoderName:error: instead");
/// Returns an initialized \c SFBAudioDecoder object for the given URL or \c nil on failure
/// @param url The URL
/// @param decoderName The name of the decoder to use
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioDecoder object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error;

/// Returns an initialized \c SFBAudioDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param decoderName The name of the decoder to use
/// @return An initialized \c SFBAudioDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBAudioDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:decoderName:error: instead");
/// Returns an initialized \c SFBAudioDecoder object for the given input source or \c nil on failure
/// @param inputSource The input source
/// @param decoderName The name of the decoder to use
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioDecoder object for the specified input source, or \c nil on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Opens the decoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
/// Closes the decoder
/// @param error An optional pointer to a \c NSError to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioDecoder and subclasses
extern NSErrorDomain const SFBAudioDecoderErrorDomain NS_SWIFT_NAME(AudioDecoder.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioDecoder
typedef NS_ERROR_ENUM(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCode) {
	/// Internal decoder error
	SFBAudioDecoderErrorCodeInternalError	= 0,
	/// Unknown decoder name
	SFBAudioDecoderErrorCodeUnknownDecoder	= 1,
	/// Invalid, unknown, or unsupported format
	SFBAudioDecoderErrorCodeInvalidFormat	= 2
} NS_SWIFT_NAME(AudioDecoder.ErrorCode);

#pragma mark - FLAC Decoder Properties

/// FLAC minimum block size (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMinimumBlockSize;
/// FLAC maximum block size (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMaximumBlockSize;
/// FLAC minimum frame size (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMinimumFrameSize;
/// FLAC maximum frame size (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMaximumFrameSize;
/// FLAC sample rate in Hz (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACSampleRate;
/// FLAC channels (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACChannels;
/// FLAC bits per sample (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACBitsPerSample;
/// FLAC total samples (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACTotalSamples;
/// FLAC MD5 sum (\c NSData)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMD5Sum;

#pragma mark - Monkey's Audio Decoder Properties

/// Monkey's Audio file version * 1000 (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFileVersion;
/// Monkey's Audio compression level (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioCompressionLevel;
/// Monkey's Audio format flags (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFormatFlags;
/// Monkey's Audio sample rate in Hz (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioSampleRate;
/// Monkey's Audio bits per sample (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBitsPerSample;
/// Monkey's Audio bytes per sample (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBytesPerSample;
/// Monkey's Audio channels (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioChannels;
/// Monkey's Audio block alignment (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlockAlignment;
/// Monkey's Audio number of blocks in a frame (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlocksPerFrame;
/// Monkey's Audio blocks in the final frame (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFinalFrameBlocks;
/// Monkey's Audio total number of frames (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalFrames;
/// Monkey's Audio header byte count of the decompressed WAV (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVHeaderBytes;
/// Monkey's Audio terminating byte count of the decompressed WAV (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTerminatingBytes;
/// Monkey's Audio data byte count of the decompressed WAV (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVDataBytes;
/// Monkey's Audio total byte count of the decompressed WAV (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTotalBytes ;
/// Monkey's Audio total byte count of the API file (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPETotalBytes;
/// Monkey's Audio total blocks of audio data (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks;
/// Monkey's Audio length in milliseconds (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds;
/// Monkey's Audio average bitrate of the APE (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate;
/// Monkey's Audio bitrate of the decompressed WAV (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioDecompressedBitrate;
/// Monkey's Audio @c YES if this is an APL file (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPL;

#pragma mark - Musepack Decoder Properties

/// Musepack sample frequency in Hz (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackSampleFrequency;
/// Musepack number of channels (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackChannels;
/// Musepack stream version (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackStreamVersion;
/// Musepack bitrate in bits per second (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackBitrate;
/// Musepack average bitrate in bits per second (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackAverageBitrate;
/// Musepack maximum band index used (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackMaximumBandIndex;
/// Musepack mid/side stereo (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackMidSideStereo;
/// Musepack supports fast seeking (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeystreamInfoMusepackFastSeek;
/// Musepack block power (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackBlockPower;

/// Musepack title ReplayGain (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTitleGain;
/// Musepack album ReplayGain (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackAlbumGain;
/// Musepack peak album loudness level (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackAlbumPeak;
/// Musepack peak title loudness level (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTitlePeak;

/// Musepack true gapless (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackIsTrueGapless;
/// Musepack number of samples (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackSamples;
/// Musepack number of leading samples that must be skipped (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackBeginningSilence;

/// Musepack version of encoder used (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackEncoderVersion;
/// Musepack encoder name (\c NSString)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackEncoder;
/// Musepack PNS used (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackPNS;
/// Musepack quality profile (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackProfile;
/// Musepack name of profile (\c NSString)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackProfileName;

/// Musepack byte offset of header position (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackHeaderPosition;
/// Musepack byte offset to file tags (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTagOffset;
/// Musepack total file length in bytes (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTotalFileLength;

#pragma mark - Ogg Opus Decoder Properties

/// Ogg Opus format version (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusVersion;
/// Ogg Opus channel count (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusChannelCount;
/// Ogg Opus number of samples to discard from the beginning of the stream (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusPreSkip;
/// Ogg Opus sample rate of the original input (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusInputSampleRate;
/// Ogg Opus gain to apply to decoded output in dB (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusOutputGain;
/// Ogg Opus channel mapping family (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusMappingFamily;
/// Ogg Opus number of Opus streams in each Ogg packet (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusStreamCount;
/// Ogg Opus number of coupled Opus streams in each Ogg packet (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusCoupledCount;
/// Ogg Opus mapping from coded stream channels to output channels (\c NSData)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusMapping;

#pragma mark - Ogg Speex Decoder Properties

/// Ogg Speex Speex string, always @c "Speex   "  (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexSpeexString;
/// Ogg Speex Speex version (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexSpeexVersion;
/// Ogg Speex Speex version ID (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexSpeexVersionID;
/// Ogg Speex total size of the header (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexHeaderSize;
/// Ogg Speex sampling rate (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexRate;
/// Ogg Speex mode used (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexMode;
/// Ogg Speex version ID of the bitstream (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexModeBitstreamVersion;
/// Ogg Speex number of channels encoded (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexNumberChannels;
/// Ogg Speex bitrate used (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexBitrate;
/// Ogg Speex size of frames (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexFrameSize;
/// Ogg Speex whether encoding is VBR (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexVBR;
/// Ogg Speex number of frames stored per Ogg packet (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexFramesPerPacket;
/// Ogg Speex number of additional headers after the comments (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexExtraHeaders;

#pragma mark - Ogg Vorbis Decoder Properties

/// Ogg Vorbis version (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisVersion;
/// Ogg Vorbis channels (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisChannels;
/// Ogg Vorbis sample rate in Hz (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisRate;
/// Ogg Vorbis bitrate upper limit (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateUpper;
/// Ogg Vorbis nominal bitrate (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateNominal;
/// Ogg Vorbis bitrate lower limit (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateLower;
/// Ogg Vorbis bitrate window (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateWindow;

#pragma mark - True Audio Decoder Properties

/// True Audio format (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioFormat;
/// True Audio number of channels (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioNumberChannels;
/// True Audio bits per sample (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioBitsPerSample;
/// True Audio sample rate in Hz (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioSampleRate;
/// True Audio number of samples (\c NSNumber)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioSamples;

NS_ASSUME_NONNULL_END
