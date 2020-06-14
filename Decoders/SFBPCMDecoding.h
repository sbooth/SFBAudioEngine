/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDecoding.h"

NS_ASSUME_NONNULL_BEGIN

/// Value representing an invalid or unknown audio frame position
extern const AVAudioFramePosition SFBUnknownFramePosition NS_SWIFT_NAME(UnknownFramePosition);
#define SFB_UNKNOWN_FRAME_POSITION ((AVAudioFramePosition)-1)

/// Value representing an invalid or unknown audio frame length
extern const AVAudioFramePosition SFBUnknownFrameLength NS_SWIFT_NAME(UnknownFrameLength);
#define SFB_UNKNOWN_FRAME_LENGTH ((AVAudioFramePosition)-1)

NS_SWIFT_NAME(PCMDecoding) @protocol SFBPCMDecoding <SFBAudioDecoding>

#pragma mark - Position and Length Information

/// Returns the decoder's current frame position or \c SFBUnknownFramePosition if unknown
@property (nonatomic, readonly) AVAudioFramePosition framePosition NS_SWIFT_NAME(position);

/// Returns the decoder's length in frames or \c SFBUnknownFrameLength if unknown
@property (nonatomic, readonly) AVAudioFramePosition frameLength NS_SWIFT_NAME(length);

#pragma mark - Decoding

/// Decodes audio
/// @param buffer A buffer to receive the decoded audio
/// @param frameLength The desired number of audio frames
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error NS_SWIFT_NAME(decode(into:length:));

#pragma mark - Seeking

/// Seeks to the specified frame
/// @param frame The desired frame
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error NS_SWIFT_NAME(seek(to:));

@end

NS_ASSUME_NONNULL_END
