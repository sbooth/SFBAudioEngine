//
// Copyright (c) 2014-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBDSDDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Constant type for decoder names
typedef NSString * SFBDSDDecoderName NS_TYPED_ENUM NS_SWIFT_NAME(DSDDecoder.Name);

/// DSDIFF
extern SFBDSDDecoderName const SFBDSDDecoderNameDSDIFF;
/// DSF
extern SFBDSDDecoderName const SFBDSDDecoderNameDSF;

/// A decoder providing audio as DSD
NS_SWIFT_NAME(DSDDecoder) @interface SFBDSDDecoder : NSObject <SFBDSDDecoding>

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

/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter detectContentType: Whether to attempt to determine the content type of `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url detectContentType:(BOOL)detectContentType error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter mimeTypeHint: A MIME type hint for `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url mimeTypeHint:(nullable NSString *)mimeTypeHint error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter detectContentType: Whether to attempt to determine the content type of `url`
/// - parameter mimeTypeHint: A MIME type hint for `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url detectContentType:(BOOL)detectContentType mimeTypeHint:(nullable NSString *)mimeTypeHint error:(NSError **)error;

/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:error: instead");
/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - important: If `detectContentType` is `YES` the input source must support seeking and will be opened for reading
/// - parameter inputSource: The input source
/// - parameter detectContentType: Whether to attempt to determine the content type of `inputSource`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource detectContentType:(BOOL)detectContentType error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter mimeTypeHint: A MIME type hint for `inputSource`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeTypeHint:(nullable NSString *)mimeTypeHint error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - important: If `detectContentType` is `YES` the input source must support seeking and will be opened for reading
/// - parameter inputSource: The input source
/// - parameter detectContentType: Whether to attempt to determine the content type of `inputSource`
/// - parameter mimeTypeHint: A MIME type hint for `inputSource`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource detectContentType:(BOOL)detectContentType mimeTypeHint:(nullable NSString *)mimeTypeHint error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBDSDDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithURL:decoderName:error: instead");
/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBDSDDecoderName)decoderName error:(NSError **)error;

/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter decoderName: The name of the decoder to use
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBDSDDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:decoderName:error: instead");
/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter decoderName: The name of the decoder to use
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBDSDDecoderName)decoderName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Opens the decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
/// Closes the decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

NS_ASSUME_NONNULL_END
