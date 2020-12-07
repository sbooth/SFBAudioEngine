/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import <SFBAudioEngine/SFBAudioEngineTypes.h>
#import <SFBAudioEngine/SFBInputSource.h>

NS_ASSUME_NONNULL_BEGIN

/// Protocol defining the interface for audio decoders
NS_SWIFT_NAME(AudioDecoding) @protocol SFBAudioDecoding

#pragma mark - Input

/// The \c SFBInputSource providing data to this decoder
@property (nonatomic, readonly) SFBInputSource *inputSource;

#pragma mark - Audio Format Information

/// The format of the encoded audio data
@property (nonatomic, readonly) AVAudioFormat *sourceFormat;

/// The format of audio data produced by `-decodeIntoBuffer:error:`
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

/// \c YES if decoding allows the original signal to be perfectly reconstructed
@property (nonatomic, readonly) BOOL decodingIsLossless;

#pragma mark - Setup and Teardown

/// Opens the decoder for reading
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the decoder
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns \c YES if the decoder is open
@property (nonatomic, readonly) BOOL isOpen;

#pragma mark - Decoding

/// Decodes audio
/// @param buffer A buffer to receive the decoded audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(decode(into:));

#pragma mark - Seeking

/// Returns \c YES if the decoder is seekable
@property (nonatomic, readonly) BOOL supportsSeeking;

@end

NS_ASSUME_NONNULL_END
