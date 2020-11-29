/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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

- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:error: instead");
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithURL:decoderName:error: instead");
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error;

- (nullable instancetype)initWithInputSource:(SFBInputSource *)outputSource decoderName:(SFBAudioDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:decoderName:error: instead");
- (nullable instancetype)initWithInputSource:(SFBInputSource *)outputSource decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioDecoder and subclasses
extern NSErrorDomain const SFBAudioDecoderErrorDomain NS_SWIFT_NAME(AudioDecoder.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioDecoder
typedef NS_ERROR_ENUM(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCode) {
	/// Invalid, unknown, or unsupported format
	SFBAudioDecoderErrorCodeInvalidFormat	= 0,
	/// Internal encoder error
	SFBAudioDecoderErrorCodeInternalError	= 1
} NS_SWIFT_NAME(AudioDecoder.ErrorCode);

NS_ASSUME_NONNULL_END
