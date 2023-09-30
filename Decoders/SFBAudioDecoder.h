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
/// FLAC MD5 sum (\c NSData)
extern SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyFLACMD5Sum;

NS_ASSUME_NONNULL_END
