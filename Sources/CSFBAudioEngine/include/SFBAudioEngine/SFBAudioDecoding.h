//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>
#import <AVFAudio/AVFAudio.h>

#import <SFBAudioEngine/SFBAudioEngineTypes.h>
#import <SFBAudioEngine/SFBInputSource.h>

NS_ASSUME_NONNULL_BEGIN

/// A key in an audio decoder's properties dictionary
typedef NSString * SFBAudioDecodingPropertiesKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioDecodingPropertiesKey);
/// A value in an audio decoder's properties dictionary
typedef id SFBAudioDecodingPropertiesValue NS_SWIFT_NAME(AudioDecodingPropertiesValue);

/// Protocol defining the interface for audio decoders
NS_SWIFT_NAME(AudioDecoding) @protocol SFBAudioDecoding

#pragma mark - Input

/// The `SFBInputSource` providing data to this decoder
@property (nonatomic, readonly) SFBInputSource *inputSource;

#pragma mark - Audio Format Information

/// The format of the encoded audio data
@property (nonatomic, readonly) AVAudioFormat *sourceFormat;

/// The format of audio data produced by `-decodeIntoBuffer:error:`
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

/// `YES` if decoding allows the original signal to be perfectly reconstructed
@property (nonatomic, readonly) BOOL decodingIsLossless;

/// Returns a dictionary containing decoder-specific properties
/// - note: Properties are read when the decoder is opened
@property (nonatomic, readonly) NSDictionary<SFBAudioDecodingPropertiesKey, SFBAudioDecodingPropertiesValue> *properties;

#pragma mark - Setup and Teardown

/// Opens the decoder for reading
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns `YES` if the decoder is open
@property (nonatomic, readonly) BOOL isOpen;

#pragma mark - Decoding

/// Decodes audio
/// - parameter buffer: A buffer to receive the decoded audio
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(decode(into:));

#pragma mark - Seeking

/// Returns `YES` if the decoder is seekable
@property (nonatomic, readonly) BOOL supportsSeeking;

#pragma mark - Error Information

/// The `NSErrorDomain` used by `SFBAudioDecoding` and subclasses
extern NSErrorDomain const SFBAudioDecodingErrorDomain NS_SWIFT_NAME(AudioDecoding.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioDecoding`
typedef NS_ERROR_ENUM(SFBAudioDecodingErrorDomain, SFBAudioDecodingErrorCode) {
	/// Invalid or unknown format
	SFBAudioDecodingErrorCodeInvalidFormat		= 0,
	/// Unsupported format
	SFBAudioDecodingErrorCodeUnsupportedFormat	= 1,
	/// Internal decoder error
	SFBAudioDecodingErrorCodeInternalError		= 2,
	/// Decoding error
	SFBAudioDecodingErrorCodeDecodingError		= 3,
	/// Seek error
	SFBAudioDecodingErrorCodeSeekError			= 4,
} NS_SWIFT_NAME(AudioDecoding.ErrorCode);

@end

extern NSErrorUserInfoKey const SFBAudioDecodingFormatNameErrorKey; // NSString

NS_ASSUME_NONNULL_END
