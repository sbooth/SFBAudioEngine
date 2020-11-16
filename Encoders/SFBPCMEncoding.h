/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioEncoding.h>

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(PCMEncoding) @protocol SFBPCMEncoding <SFBAudioEncoding>

#pragma mark - Position and Length Information

/// Returns the encoder's current frame position or \c SFBUnknownFramePosition if unknown
@property (nonatomic, readonly) AVAudioFramePosition framePosition NS_SWIFT_NAME(position);

#pragma mark - Encoding

/// Encodes audio
/// @param buffer A buffer containing the audio to encode
/// @param frameLength The desired number of audio frames
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error NS_SWIFT_NAME(encode(from:length:));

@end

NS_ASSUME_NONNULL_END
