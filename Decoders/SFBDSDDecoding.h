/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Protocol defining the interface for audio encoders producing DSD audio
NS_SWIFT_NAME(DSDDecoding) @protocol SFBDSDDecoding <SFBAudioDecoding>

#pragma mark - Position and Length Information

/// Returns the decoder's current packet position or \c SFBUnknownPacketPosition if unknown
@property (nonatomic, readonly) AVAudioFramePosition packetPosition NS_SWIFT_NAME(position);

/// Returns the decoder's length in packets or \c SFBUnknownPacketCount if unknown
@property (nonatomic, readonly) AVAudioFramePosition packetCount NS_SWIFT_NAME(count);

#pragma mark - Decoding

/// Decodes audio
/// @param buffer A buffer to receive the decoded audio
/// @param packetCount The desired number of audio packets
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)decodeIntoBuffer:(AVAudioCompressedBuffer *)buffer packetCount:(AVAudioPacketCount)packetCount error:(NSError **)error NS_SWIFT_NAME(decode(into:count:));

#pragma mark - Seeking

/// Returns \c YES if the decoder is seekable
@property (nonatomic, readonly) BOOL supportsSeeking;

/// Seeks to the specified packet
/// @param packet The desired packet
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)seekToPacket:(AVAudioFramePosition)packet error:(NSError **)error NS_SWIFT_NAME(seek(to:));

@end

NS_ASSUME_NONNULL_END
