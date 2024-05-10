//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>
#import <AVFAudio/AVFAudio.h>

#import <SFBAudioEngine/SFBAudioEngineTypes.h>
#import <SFBAudioEngine/SFBInputSource.h>

NS_ASSUME_NONNULL_BEGIN

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

@end

NS_ASSUME_NONNULL_END
