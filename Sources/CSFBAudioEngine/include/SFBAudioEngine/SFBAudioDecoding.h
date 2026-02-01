//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBAudioEngineTypes.h>
#import <SFBAudioEngine/SFBInputSource.h>

#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// A key in an audio decoder's properties dictionary
typedef NSString *SFBAudioDecodingPropertiesKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioDecodingPropertiesKey);
/// A value in an audio decoder's properties dictionary
typedef id SFBAudioDecodingPropertiesValue NS_SWIFT_NAME(AudioDecodingPropertiesValue);

/// Protocol defining the interface for audio decoders
NS_SWIFT_NAME(AudioDecoding)
@protocol SFBAudioDecoding

// MARK: - Input

/// The input source providing data to this decoder
@property(nonatomic, readonly) SFBInputSource *inputSource;

// MARK: - Audio Format Information

/// The format of the encoded audio data
@property(nonatomic, readonly) AVAudioFormat *sourceFormat;

/// The format of audio data produced by ``-decodeIntoBuffer:error:``
@property(nonatomic, readonly) AVAudioFormat *processingFormat;

/// `YES` if decoding allows the original signal to be perfectly reconstructed
@property(nonatomic, readonly) BOOL decodingIsLossless;

/// Returns a dictionary containing decoder-specific properties
/// - note: Properties are read when the decoder is opened
@property(nonatomic, readonly) NSDictionary<SFBAudioDecodingPropertiesKey, SFBAudioDecodingPropertiesValue> *properties;

// MARK: - Setup and Teardown

/// Opens the decoder for reading
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the decoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// `YES` if the decoder is open
@property(nonatomic, readonly) BOOL isOpen;

// MARK: - Decoding

/// Decodes audio
/// - parameter buffer: A buffer to receive the decoded audio
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(decode(into:));

// MARK: - Seeking

/// `YES` if the decoder is seekable
@property(nonatomic, readonly) BOOL supportsSeeking;

@end

NS_ASSUME_NONNULL_END
