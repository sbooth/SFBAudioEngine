//
// Copyright (c) 2020 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>
#import <AVFAudio/AVFAudio.h>

#import <SFBAudioEngine/SFBAudioEngineTypes.h>
#import <SFBAudioEngine/SFBOutputSource.h>

NS_ASSUME_NONNULL_BEGIN

/// A key in an audio encoder's settings dictionary
typedef NSString * SFBAudioEncodingSettingsKey NS_TYPED_ENUM NS_SWIFT_NAME(AudioEncodingSettingsKey);
/// A value in an audio encoder's settings dictionary
typedef id SFBAudioEncodingSettingsValue NS_SWIFT_NAME(AudioEncodingSettingsValue);

/// Protocol defining the interface for audio encoders
NS_SWIFT_NAME(AudioEncoding) @protocol SFBAudioEncoding

#pragma mark - Output

/// The `SFBOutputSource` consuming data from this decoder
@property (nonatomic, readonly) SFBOutputSource *outputSource;

#pragma mark - Audio Format Information

/// The source audio format
@property (nonatomic, readonly) AVAudioFormat *sourceFormat;

/// The format of audio of data consumed by `-encodeFromBuffer`:error:
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

/// The format of the encoded audio data
@property (nonatomic, readonly) AVAudioFormat *outputFormat;

/// `YES` if encoding allows the original signal to be perfectly reconstructed
@property (nonatomic, readonly) BOOL encodingIsLossless;

/// Returns the processing format used for the given source format
/// - parameter sourceFormat: The source audio format
/// - returns: The processing format corresponding to `sourceFormat`, or `nil` if `sourceFormat` is not supported
- (nullable AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat;

#pragma mark - Setup and Teardown

/// Sets the source audio format for the encoder
/// - note: If supported, the source format is used  to determine the appropriate `processingFormat`
/// - parameter sourceFormat: The source audio format
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)setSourceFormat:(AVAudioFormat *)sourceFormat error:(NSError **)error;

/// Opens the encoder for writing
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)openReturningError:(NSError **)error;

/// Closes the encoder
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns `YES` if the encoder is open
@property (nonatomic, readonly) BOOL isOpen;

#pragma mark - Encoding

/// Encoder settings
@property (nonatomic, copy, nullable) NSDictionary<SFBAudioEncodingSettingsKey, SFBAudioEncodingSettingsValue> *settings;

/// Encodes audio
/// - parameter buffer: A buffer to receive the decoded audio
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)encodeFromBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(encode(from:));

/// Finishes encoding
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)finishEncodingReturningError:(NSError **)error NS_SWIFT_NAME(finish());

@end

NS_ASSUME_NONNULL_END
