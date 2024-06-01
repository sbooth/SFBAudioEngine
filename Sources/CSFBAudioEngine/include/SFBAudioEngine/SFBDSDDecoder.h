//
// Copyright (c) 2014-2024 Stephen F. Booth <me@sbooth.org>
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
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter mimeType: The MIME type of `url` or `nil`
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:error: instead");
/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
/// Returns an initialized `SFBDSDDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter mimeType: The MIME type of `inputSource` or `nil`
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - returns: An initialized `SFBDSDDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url decoderName:(SFBDSDDecoderName)decoderName NS_SWIFT_UNAVAILABLE("Use -initWithURL:decoderName:error: instead");
/// Returns an initialized `SFBDSDDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter decoderName: The name of the decoder to use
/// - parameter error: An optional pointer to a `NSError` to receive error information
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
/// - parameter error: An optional pointer to a `NSError` to receive error information
/// - returns: An initialized `SFBDSDDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBDSDDecoderName)decoderName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

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

/// The `NSErrorDomain` used by `SFBDSDDecoder` and subclasses
extern NSErrorDomain const SFBDSDDecoderErrorDomain NS_SWIFT_NAME(DSDDecoder.ErrorDomain);

/// Possible `NSError` error codes used by `SFBDSDDecoder`
typedef NS_ERROR_ENUM(SFBDSDDecoderErrorDomain, SFBDSDDecoderErrorCode) {
	/// Internal decoder error
	SFBDSDDecoderErrorCodeInternalError		= 0,
	/// Unknown decoder name
	SFBDSDDecoderErrorCodeUnknownDecoder	= 1,
	/// Invalid, unknown, or unsupported format
	SFBDSDDecoderErrorCodeInvalidFormat		= 2
} NS_SWIFT_NAME(DSDDecoder.ErrorCode);

NS_ASSUME_NONNULL_END

