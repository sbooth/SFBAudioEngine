/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import <SFBAudioEngine/SFBOutputSource.h>

NS_ASSUME_NONNULL_BEGIN

#pragma mark Audio Format Identifiers

NS_SWIFT_NAME(AudioEncoding) @protocol SFBAudioEncoding

#pragma mark - Output

/// The \c SFBOutputSource consuming data from this decoder
@property (nonatomic, readonly) SFBOutputSource *outputSource;

#pragma mark - Audio Format Information

/// The format of audio of data consumed by `-encodeFromBuffer:error:`
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

/// The format of the encoded audio data
@property (nonatomic, readonly) AVAudioFormat *outputFormat;

#pragma mark - Setup and Teardown

/// Opens the encoder for writing
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the encoder
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns \c YES if the encoder is open
@property (nonatomic, readonly) BOOL isOpen;

#pragma mark - Encoding

/// Encodes audio
/// @param buffer A buffer to receive the decoded audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)encodeFromBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(encode(from:));

#pragma mark - Seeking

/// Returns \c YES if the encoder is seekable
@property (nonatomic, readonly) BOOL supportsSeeking;

@end

NS_ASSUME_NONNULL_END
