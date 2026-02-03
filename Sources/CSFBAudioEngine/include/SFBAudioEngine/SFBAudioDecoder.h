//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <SFBAudioEngine/SFBPCMDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Constant type for decoder names
typedef NSString *SFBAudioDecoderName NS_TYPED_ENUM NS_SWIFT_NAME(AudioDecoder.Name);

/// FLAC
extern SFBAudioDecoderName const SFBAudioDecoderNameFLAC;
/// Monkey's Audio
extern SFBAudioDecoderName const SFBAudioDecoderNameMonkeysAudio;
/// Module
extern SFBAudioDecoderName const SFBAudioDecoderNameModule;
/// MPEG 1/2/2.5 Layers I, II, and III
extern SFBAudioDecoderName const SFBAudioDecoderNameMPEG;
/// Musepack
extern SFBAudioDecoderName const SFBAudioDecoderNameMusepack;
/// Ogg FLAC
extern SFBAudioDecoderName const SFBAudioDecoderNameOggFLAC;
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
NS_SWIFT_NAME(AudioDecoder)
@interface SFBAudioDecoder : NSObject <SFBPCMDecoding>

// MARK: - File Format Support

/// A set containing the supported path extensions
@property(class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/// A set containing the supported MIME types
@property(class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/// Tests whether a file extension is supported
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/// Tests whether a MIME type is supported
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

// MARK: - Creation

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter detectContentType: Whether to attempt to determine the content type of `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url detectContentType:(BOOL)detectContentType error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter mimeTypeHint: A MIME type hint for `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url
                        mimeTypeHint:(nullable NSString *)mimeTypeHint
                               error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter detectContentType: Whether to attempt to determine the content type of `url`
/// - parameter mimeTypeHint: A MIME type hint for `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url
                   detectContentType:(BOOL)detectContentType
                        mimeTypeHint:(nullable NSString *)mimeTypeHint
                               error:(NSError **)error;

/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource
        NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - important: If `detectContentType` is `YES` the input source must support seeking and will be opened for reading
/// - parameter inputSource: The input source
/// - parameter detectContentType: Whether to attempt to determine the content type of `inputSource`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource
                           detectContentType:(BOOL)detectContentType
                                       error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter mimeTypeHint: The MIME type of `inputSource` or `nil`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource
                                mimeTypeHint:(nullable NSString *)mimeTypeHint
                                       error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - important: If `detectContentType` is `YES` the input source must support seeking and will be opened for reading
/// - parameter inputSource: The input source
/// - parameter detectContentType: Whether to attempt to determine the content type of `inputSource`
/// - parameter mimeTypeHint: A MIME type hint for `inputSource`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource
                           detectContentType:(BOOL)detectContentType
                                mimeTypeHint:(nullable NSString *)mimeTypeHint
                                       error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url
                         decoderName:(SFBAudioDecoderName)decoderName
        NS_SWIFT_UNAVAILABLE("Use -initWithURL:decoderName:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error;

/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter decoderName: The name of the decoder to use
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource
                                 decoderName:(SFBAudioDecoderName)decoderName
        NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:decoderName:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter decoderName: The name of the decoder to use
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource
                                 decoderName:(SFBAudioDecoderName)decoderName
                                       error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Opens the decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
/// Closes the decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

// MARK: - Error Information

/// The `NSErrorDomain` used by `SFBAudioDecoder` and subclasses
extern NSErrorDomain const SFBAudioDecoderErrorDomain NS_SWIFT_NAME(AudioDecoder.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioDecoder`
typedef NS_ERROR_ENUM(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCode){
    /// Unknown decoder name
    SFBAudioDecoderErrorCodeUnknownDecoder = 0,
    /// Invalid or unknown format
    SFBAudioDecoderErrorCodeInvalidFormat = 1,
    /// Unsupported format
    SFBAudioDecoderErrorCodeUnsupportedFormat = 2,
    /// Internal decoder error
    SFBAudioDecoderErrorCodeInternalError = 3,
    /// Decoding error
    SFBAudioDecoderErrorCodeDecodingError = 4,
    /// Seek error
    SFBAudioDecoderErrorCodeSeekError = 5,
} NS_SWIFT_NAME(AudioDecoder.Error);

// MARK: - FLAC and Ogg FLAC Decoder Properties

/// FLAC minimum block size (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMinimumBlockSize;
/// FLAC maximum block size (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMaximumBlockSize;
/// FLAC minimum frame size (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMinimumFrameSize;
/// FLAC maximum frame size (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMaximumFrameSize;
/// FLAC sample rate in Hz (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACSampleRate;
/// FLAC channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACChannels;
/// FLAC bits per sample (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACBitsPerSample;
/// FLAC total samples (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACTotalSamples;
/// FLAC MD5 sum (`NSData`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMD5Sum;

// MARK: - Monkey's Audio Decoder Properties

/// Monkey's Audio file version * 1000 (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFileVersion;
/// Monkey's Audio compression level (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioCompressionLevel;
/// Monkey's Audio format flags (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFormatFlags;
/// Monkey's Audio sample rate in Hz (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioSampleRate;
/// Monkey's Audio bits per sample (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBitsPerSample;
/// Monkey's Audio bytes per sample (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBytesPerSample;
/// Monkey's Audio channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioChannels;
/// Monkey's Audio block alignment (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlockAlignment;
/// Monkey's Audio number of blocks in a frame (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlocksPerFrame;
/// Monkey's Audio blocks in the final frame (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFinalFrameBlocks;
/// Monkey's Audio total number of frames (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalFrames;
/// Monkey's Audio header byte count of the decompressed WAV (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVHeaderBytes;
/// Monkey's Audio terminating byte count of the decompressed WAV (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTerminatingBytes;
/// Monkey's Audio data byte count of the decompressed WAV (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVDataBytes;
/// Monkey's Audio total byte count of the decompressed WAV (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTotalBytes;
/// Monkey's Audio total byte count of the API file (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPETotalBytes;
/// Monkey's Audio total blocks of audio data (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks;
/// Monkey's Audio length in milliseconds (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds;
/// Monkey's Audio average bitrate of the APE (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate;
/// Monkey's Audio bitrate of the decompressed WAV (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioDecompressedBitrate;
/// Monkey's Audio `YES` if this is an APL file (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPL;

// MARK: - Musepack Decoder Properties

/// Musepack sample frequency in Hz (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackSampleFrequency;
/// Musepack number of channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackChannels;
/// Musepack stream version (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackStreamVersion;
/// Musepack bitrate in bits per second (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackBitrate;
/// Musepack average bitrate in bits per second (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackAverageBitrate;
/// Musepack maximum band index used (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackMaximumBandIndex;
/// Musepack mid/side stereo (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackMidSideStereo;
/// Musepack supports fast seeking (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeystreamInfoMusepackFastSeek;
/// Musepack block power (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackBlockPower;

/// Musepack title ReplayGain (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTitleGain;
/// Musepack album ReplayGain (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackAlbumGain;
/// Musepack peak album loudness level (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackAlbumPeak;
/// Musepack peak title loudness level (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTitlePeak;

/// Musepack true gapless (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackIsTrueGapless;
/// Musepack number of samples (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackSamples;
/// Musepack number of leading samples that must be skipped (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackBeginningSilence;

/// Musepack version of encoder used (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackEncoderVersion;
/// Musepack encoder name (`NSString`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackEncoder;
/// Musepack PNS used (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackPNS;
/// Musepack quality profile (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackProfile;
/// Musepack name of profile (`NSString`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackProfileName;

/// Musepack byte offset of header position (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackHeaderPosition;
/// Musepack byte offset to file tags (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTagOffset;
/// Musepack total file length in bytes (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMusepackTotalFileLength;

// MARK: - Ogg Opus Decoder Properties

/// Ogg Opus format version (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusVersion;
/// Ogg Opus channel count (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusChannelCount;
/// Ogg Opus number of samples to discard from the beginning of the stream (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusPreSkip;
/// Ogg Opus sample rate of the original input (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusInputSampleRate;
/// Ogg Opus gain to apply to decoded output in dB (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusOutputGain;
/// Ogg Opus channel mapping family (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusMappingFamily;
/// Ogg Opus number of Opus streams in each Ogg packet (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusStreamCount;
/// Ogg Opus number of coupled Opus streams in each Ogg packet (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusCoupledCount;
/// Ogg Opus mapping from coded stream channels to output channels (`NSData`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggOpusMapping;

// MARK: - Ogg Speex Decoder Properties

/// Ogg Speex Speex string, always `"Speex   "`  (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexSpeexString;
/// Ogg Speex Speex version (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexSpeexVersion;
/// Ogg Speex Speex version ID (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexSpeexVersionID;
/// Ogg Speex total size of the header (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexHeaderSize;
/// Ogg Speex sampling rate (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexRate;
/// Ogg Speex mode used (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexMode;
/// Ogg Speex version ID of the bitstream (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexModeBitstreamVersion;
/// Ogg Speex number of channels encoded (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexNumberChannels;
/// Ogg Speex bitrate used (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexBitrate;
/// Ogg Speex size of frames (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexFrameSize;
/// Ogg Speex whether encoding is VBR (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexVBR;
/// Ogg Speex number of frames stored per Ogg packet (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexFramesPerPacket;
/// Ogg Speex number of additional headers after the comments (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggSpeexExtraHeaders;

// MARK: - Ogg Vorbis Decoder Properties

/// Ogg Vorbis version (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisVersion;
/// Ogg Vorbis channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisChannels;
/// Ogg Vorbis sample rate in Hz (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisRate;
/// Ogg Vorbis bitrate upper limit (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateUpper;
/// Ogg Vorbis nominal bitrate (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateNominal;
/// Ogg Vorbis bitrate lower limit (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateLower;
/// Ogg Vorbis bitrate window (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyOggVorbisBitrateWindow;

// MARK: - Shorten Decoder Properties

/// Shorten version (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenVersion;
/// Shorten file type (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenFileType;
/// Shorten number of channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenNumberChannels;
/// Shorten block size (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBlockSize;
/// Shorten sample rate in Hz (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenSampleRate;
/// Shorten bits per sample (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBitsPerSample;
/// Shorten big endian (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBigEndian;

// MARK: - True Audio Decoder Properties

/// True Audio format (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioFormat;
/// True Audio number of channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioNumberChannels;
/// True Audio bits per sample (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioBitsPerSample;
/// True Audio sample rate in Hz (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioSampleRate;
/// True Audio number of samples (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyTrueAudioSamples;

// MARK: - WavPack Decoder Properties

/// WavPack mode (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackMode;
/// WavPack qualify mode (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackQualifyMode;
/// WavPack version (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackVersion;
/// WavPack file format (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackFileFormat;
/// WavPack number samples (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNumberSamples;
/// WavPack number samples in frame (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNumberSamplesInFrame;
/// WavPack sample rate in Hz (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackSampleRate;
/// WavPack native sample rate (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNativeSampleRate;
/// WavPack bits per sample (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackBitsPerSample;
/// WavPack bytes per sample (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackBytesPerSample;
/// WavPack number channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackNumberChannels;
/// WavPack channel mask (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackChannelMask;
/// WavPack reduced channels (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackReducedChannels;
/// WavPack float normalization exponent (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackFloatNormExponent;
/// WavPack compression ratio (`NSNumber`)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyWavPackRatio;

NS_ASSUME_NONNULL_END
