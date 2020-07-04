/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// DSD sample rates (named as multiples of the CD sample rate, 44,100 Hz)
typedef NS_ENUM(NSUInteger, SFBDSDSampleRate) {
	/// DSD (DSD64)
	SFBDSDSampleRateDSD64 	= 2822400,
	/// Double-rate DSD (DSD128)
	SFBDSDSampleRateDSD128 	= 5644800,
	/// Quad-rate DSD (DSD256)
	SFBDSDSampleRateDSD256 	= 11289600,
	/// Octuple-rate DSD (DSD512)
	SFBDSDSampleRateDSD512 	= 22579200
} NS_SWIFT_NAME(DSDSampleRate);

/// DSD sample rate variants based on 48,000 Hz
typedef NS_ENUM(NSUInteger, SFBDSDSampleRateVariant) {
	/// DSD (DSD64)
	SFBDSDSampleRateVariantDSD64 	= 3072000,
	/// Double-rate DSD (DSD128)
	SFBDSDSampleRateVariantDSD128 	= 6144000,
	/// Quad-rate DSD (DSD256)
	SFBDSDSampleRateVariantDSD256 	= 12288000,
	/// Octuple-rate DSD (DSD512)
	SFBDSDSampleRateVariantDSD512 	= 24576000
} NS_SWIFT_NAME(DSDSampleRateVariant);

// A DSD packet in this context is 8 one-bit samples (a single channel byte) grouped into
// a clustered frame consisting of one channel byte per channel.
// From a bit perspective, for stereo one clustered frame looks like LLLLLLLLRRRRRRRR
// Since DSD audio is CBR, one packet equals one frame

/// The number of frames in a DSD packet (a clustered frame)
extern const NSInteger SFBPCMFramesPerDSDPacket NS_SWIFT_NAME(PCMFramesPerDSDPacket);
#define SFB_PCM_FRAMES_PER_DSD_PACKET 8

/// The number of bytes in a DSD packet, per channel (a channel byte)
extern const NSInteger SFBBytesPerDSDPacketPerChannel NS_SWIFT_NAME(BytesPerDSDPacketPerChannel);
#define SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL 1

/// Value representing an invalid or unknown audio packet position
extern const AVAudioFramePosition SFBUnknownPacketPosition NS_SWIFT_NAME(UnknownPacketPosition);
#define SFB_UNKNOWN_PACKET_POSITION ((AVAudioFramePosition)-1)

/// Value representing an invalid or unknown audio packet count
extern const AVAudioFramePosition SFBUnknownPacketCount NS_SWIFT_NAME(UnknownPacketCount);
#define SFB_UNKNOWN_PACKET_COUNT ((AVAudioFramePosition)-1)

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
