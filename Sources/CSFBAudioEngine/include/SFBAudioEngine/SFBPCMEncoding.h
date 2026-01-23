//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBAudioEncoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Protocol defining the interface for audio encoders consuming PCM audio
NS_SWIFT_NAME(PCMEncoding)
@protocol SFBPCMEncoding<SFBAudioEncoding>

#pragma mark - Position and Length Information

/// Returns the encoder's current frame position or `SFBUnknownFramePosition` if unknown
@property(nonatomic, readonly) AVAudioFramePosition framePosition NS_SWIFT_NAME(position);

#pragma mark - Encoding

/// The estimated number of frames to encode or `0` if unknown
@property(nonatomic) AVAudioFramePosition estimatedFramesToEncode;

/// Encodes audio
/// - parameter buffer: A buffer containing the audio to encode
/// - parameter frameLength: The desired number of audio frames
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer
             frameLength:(AVAudioFrameCount)frameLength
                   error:(NSError **)error NS_SWIFT_NAME(encode(from:length:));

@end

NS_ASSUME_NONNULL_END
