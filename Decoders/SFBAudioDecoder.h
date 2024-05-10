//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
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

/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter mimeType: The MIME type of `url` or `nil`
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter mimeType: The MIME type of `inputSource` or `nil`
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithURL:decoderName:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error;

/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter decoderName: The name of the decoder to use
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBAudioDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:decoderName:error: instead");
/// Returns an initialized `SFBAudioDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter decoderName: The name of the decoder to use
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBAudioDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Opens the decoder
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
/// Closes the decoder
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

#pragma mark - Error Information

/// The `NSErrorDomain` used by `SFBAudioDecoder` and subclasses
extern NSErrorDomain const SFBAudioDecoderErrorDomain NS_SWIFT_NAME(AudioDecoder.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioDecoder`
typedef NS_ERROR_ENUM(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCode) {
	/// Internal decoder error
	SFBAudioDecoderErrorCodeInternalError	= 0,
	/// Unknown decoder name
	SFBAudioDecoderErrorCodeUnknownDecoder	= 1,
	/// Invalid, unknown, or unsupported format
	SFBAudioDecoderErrorCodeInvalidFormat	= 2
} NS_SWIFT_NAME(AudioDecoder.ErrorCode);

NS_ASSUME_NONNULL_END
