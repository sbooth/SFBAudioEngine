//
// SPDX-FileCopyrightText: 2014 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <SFBAudioEngine/SFBAudioDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// Protocol defining the interface for audio decoders producing DSD audio
NS_SWIFT_NAME(DSDDecoding)
@protocol SFBDSDDecoding <SFBAudioDecoding>

// MARK: - Position and Length Information

/// The decoder's current packet position or `SFBUnknownPacketPosition` if unknown
@property(nonatomic, readonly) AVAudioFramePosition packetPosition NS_SWIFT_NAME(position);

/// The decoder's length in packets or `SFBUnknownPacketCount` if unknown
@property(nonatomic, readonly) AVAudioFramePosition packetCount NS_SWIFT_NAME(count);

// MARK: - Decoding

/// Decodes audio
/// - parameter buffer: A buffer to receive the decoded audio
/// - parameter packetCount: The desired number of audio packets
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)decodeIntoBuffer:(AVAudioCompressedBuffer *)buffer
             packetCount:(AVAudioPacketCount)packetCount
                   error:(NSError **)error NS_SWIFT_NAME(decode(into:count:));

// MARK: - Seeking

/// Seeks to the specified packet
/// - parameter packet: The desired packet
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
- (BOOL)seekToPacket:(AVAudioFramePosition)packet error:(NSError **)error NS_SWIFT_NAME(seek(to:));

@end

NS_ASSUME_NONNULL_END
