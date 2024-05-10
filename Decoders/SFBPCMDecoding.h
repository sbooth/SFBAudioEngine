//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBAudioDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Protocol defining the interface for audio decoders producing PCM audio
NS_SWIFT_NAME(PCMDecoding) @protocol SFBPCMDecoding <SFBAudioDecoding>

#pragma mark - Position and Length Information

/// Returns the decoder's current frame position or `SFBUnknownFramePosition` if unknown
@property (nonatomic, readonly) AVAudioFramePosition framePosition NS_SWIFT_NAME(position);

/// Returns the decoder's length in frames or `SFBUnknownFrameLength` if unknown
@property (nonatomic, readonly) AVAudioFramePosition frameLength NS_SWIFT_NAME(length);

#pragma mark - Decoding

/// Decodes audio
/// - parameter buffer: A buffer to receive the decoded audio
/// - parameter frameLength: The desired number of audio frames
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error NS_SWIFT_NAME(decode(into:length:));

#pragma mark - Seeking

/// Seeks to the specified frame
/// - parameter frame: The desired frame
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error NS_SWIFT_NAME(seek(to:));

@end

NS_ASSUME_NONNULL_END
