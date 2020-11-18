/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import <SFBAudioEngine/SFBOutputSource.h>

NS_ASSUME_NONNULL_BEGIN

typedef NSString * SFBAudioEncodingSettingsKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioEncodingSettingsKey);

NS_SWIFT_NAME(AudioEncoding) @protocol SFBAudioEncoding

#pragma mark - Output

/// The \c SFBOutputSource consuming data from this decoder
@property (nonatomic, readonly) SFBOutputSource *outputSource;

#pragma mark - Audio Format Information

/// The source audio format
@property (nonatomic, readonly) AVAudioFormat *sourceFormat;

/// The format of audio of data consumed by \c -encodeFromBuffer:error:
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

/// The format of the encoded audio data
@property (nonatomic, readonly) AVAudioFormat *outputFormat;

/// \c YES if encoding allows the original signal to be perfectly reconstructed
@property (nonatomic, readonly) BOOL encodingIsLossless;

/// Returns the processing format used for the given source format
/// @param sourceFormat The source audio format
/// @return The processing format corresponding to \c sourceFormat, or \c nil if \c sourceFormat is not supported
- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat;

#pragma mark - Setup and Teardown

/// Sets the source audio format for the encoder, uses it to determine the appropriate \c processingFormat, and opens the encoder for writing
/// @note Most encoders do not support all possible source formats
/// @param sourceFormat The source audio format
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)openWithSourceFormat:(AVAudioFormat *)sourceFormat error:(NSError **)error;

/// Finishes encoding and closes the encoder
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns \c YES if the encoder is open
@property (nonatomic, readonly) BOOL isOpen;

#pragma mark - Encoding

/// Encoder settings
@property (nonatomic, copy, nullable) NSDictionary<SFBAudioEncodingSettingsKey, id> *settings;

/// Encodes audio
/// @param buffer A buffer to receive the decoded audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)encodeFromBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(encode(from:));

@end

NS_ASSUME_NONNULL_END
