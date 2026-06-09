//
// SPDX-FileCopyrightText: 2006 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <CoreAudioTypes/CoreAudioTypes.h>

// MARK: - Audio Format Identifiers

// clang-format off

/// Additional audio format IDs
CF_ENUM(AudioFormatID) {
    /// Direct Stream Digital (DSD)
    kSFBAudioFormatDSD CF_SWIFT_NAME(dsd) = 'DSD ',
    /// DSD over PCM (DoP)
    kSFBAudioFormatDoP CF_SWIFT_NAME(dsdOverPCM) = 'DoP ',
    /// Module
    kSFBAudioFormatModule CF_SWIFT_NAME(module) = 'MOD ',
    /// Monkey's Audio (APE)
    kSFBAudioFormatMonkeysAudio CF_SWIFT_NAME(monkeysAudio) = 'APE ',
    /// Musepack
    kSFBAudioFormatMusepack CF_SWIFT_NAME(musepack) = 'MPC ',
    /// Shorten
    kSFBAudioFormatShorten CF_SWIFT_NAME(shorten) = 'SHN ',
    /// Ogg Speex
    kSFBAudioFormatSpeex CF_SWIFT_NAME(speex) = 'SPX ',
    /// True Audio
    kSFBAudioFormatTrueAudio CF_SWIFT_NAME(trueAudio) = 'TTA ',
    /// Ogg Vorbis
    kSFBAudioFormatVorbis CF_SWIFT_NAME(vorbis) = 'VORB',
    /// WavPack
    kSFBAudioFormatWavPack CF_SWIFT_NAME(wavPack) = 'WV  ',
};

// MARK: - DSD Constants

/// DSD sample rates (named as multiples of the CD sample rate, 44,100 Hz)
CF_ENUM(uint32_t) {
    /// DSD (DSD64) based on 44,100 Hz
    kSFBSampleRateDSD64 CF_SWIFT_NAME(dsd64SampleRate) = 2822400,
    /// Double-rate DSD (DSD128) based on 44,100 Hz
    kSFBSampleRateDSD128 CF_SWIFT_NAME(dsd128SampleRate) = 5644800,
    /// Quad-rate DSD (DSD256) based on 44,100 Hz
    kSFBSampleRateDSD256 CF_SWIFT_NAME(dsd256SampleRate) = 11289600,
    /// Octuple-rate DSD (DSD512) based on 44,100 Hz
    kSFBSampleRateDSD512 CF_SWIFT_NAME(dsd512SampleRate) = 22579200,
};

/// DSD sample rate variants based on 48,000 Hz
CF_ENUM(uint32_t) {
    /// DSD (DSD64) based on 48,000 Hz
    kSFBSampleRateDSD64Variant CF_SWIFT_NAME(dsd64SampleRateVariant) = 3072000,
    /// Double-rate DSD (DSD128) based on 48,000 Hz
    kSFBSampleRateDSD128Variant CF_SWIFT_NAME(dsd128SampleRateVariant) = 6144000,
    /// Quad-rate DSD (DSD256) based on 48,000 Hz
    kSFBSampleRateDSD256Variant CF_SWIFT_NAME(dsd256SampleRateVariant) = 12288000,
    /// Octuple-rate DSD (DSD512) based on 48,000 Hz
    kSFBSampleRateDSD512Variant CF_SWIFT_NAME(dsd512SampleRateVariant) = 24576000,
};

// A DSD packet in this context is 8 one-bit samples (a single channel byte) grouped into
// a clustered frame consisting of one channel byte per channel.
// From a bit perspective, for stereo one clustered frame looks like LLLLLLLLRRRRRRRR
// Since DSD audio is CBR, one packet equals one frame

CF_ENUM(int) {
    /// The number of frames in a DSD packet (a clustered frame)
    kSFBPCMFramesPerDSDPacket CF_SWIFT_NAME(pcmFramesPerDSDPacket) = 8,
    /// The number of bytes in a DSD packet, per channel (a channel byte)
    kSFBBytesPerDSDPacketPerChannel CF_SWIFT_NAME(bytesPerDSDPacketPerChannel) = 1,
};

// clang-format on

#ifdef __OBJC__

#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>

// MARK: - Constants for Unknowns

/// Value representing an invalid or unknown time
extern const NSTimeInterval SFBUnknownTime NS_SWIFT_NAME(unknownTime);

// clang-format off

/// Frame and packet unknowns
NS_ENUM(AVAudioFramePosition) {
    /// Value representing an invalid or unknown audio frame position
    SFBUnknownFramePosition NS_SWIFT_NAME(unknownFramePosition) = -1,
    /// Value representing an invalid or unknown audio frame length
    SFBUnknownFrameLength NS_SWIFT_NAME(unknownFrameLength) = -1,
    /// Value representing an invalid or unknown audio packet position
    SFBUnknownPacketPosition NS_SWIFT_NAME(unknownPacketPosition) = -1,
    /// Value representing an invalid or unknown audio packet count
    SFBUnknownPacketCount NS_SWIFT_NAME(unknownPacketCount) = -1,
};

// clang-format on

// MARK: - Playback Position

/// Playback position information
struct NS_SWIFT_SENDABLE SFBPlaybackPosition {
    /// The current frame position or `SFBUnknownFramePosition` if unknown
    AVAudioFramePosition framePosition;
    /// The total number of frames or `SFBUnknownFrameLength` if unknown
    AVAudioFramePosition frameLength;
} NS_SWIFT_NAME(PlaybackPosition);
typedef struct SFBPlaybackPosition SFBPlaybackPosition;

/// Value representing an invalid or unknown playback position
///
/// An invalid or unknown playback position is defined as `{SFBUnknownFramePosition, SFBUnknownFrameLength}`
extern const SFBPlaybackPosition SFBInvalidPlaybackPosition NS_SWIFT_NAME(PlaybackPosition.invalid);

/// Returns `YES` if both the current frame position and total number of frames in `playbackPosition` are valid
NS_INLINE BOOL SFBPlaybackPositionIsValid(SFBPlaybackPosition playbackPosition) {
    return playbackPosition.framePosition != SFBUnknownFramePosition &&
           playbackPosition.frameLength != SFBUnknownFrameLength;
}

/// Returns `YES` if either the current frame position or total number of frames in `playbackPosition` is valid
NS_INLINE BOOL SFBPlaybackPositionIsPartiallyValid(SFBPlaybackPosition playbackPosition) {
    return playbackPosition.framePosition != SFBUnknownFramePosition ||
           playbackPosition.frameLength != SFBUnknownFrameLength;
}

// MARK: - Playback Time

/// Playback time information
struct NS_SWIFT_SENDABLE SFBPlaybackTime {
    /// The current time or `SFBUnknownTime` if unknown
    NSTimeInterval currentTime;
    /// The total time or `SFBUnknownTime` if unknown
    NSTimeInterval totalTime;
} NS_SWIFT_NAME(PlaybackTime);
typedef struct SFBPlaybackTime SFBPlaybackTime;

/// Value representing an invalid or unknown playback time
///
/// An invalid or unknown playback time is defined as `{SFBUnknownTime, SFBUnknownTime}`
extern const SFBPlaybackTime SFBInvalidPlaybackTime NS_SWIFT_NAME(PlaybackTime.invalid);

/// Returns `YES` if both the current time and total time in `playbackTime` are valid
NS_INLINE BOOL SFBPlaybackTimeIsValid(SFBPlaybackTime playbackTime) {
    return playbackTime.currentTime != SFBUnknownTime && playbackTime.totalTime != SFBUnknownTime;
}

/// Returns `YES` if either the current time or total time in `playbackTime` is valid
NS_INLINE BOOL SFBPlaybackTimeIsPartiallyValid(SFBPlaybackTime playbackTime) {
    return playbackTime.currentTime != SFBUnknownTime || playbackTime.totalTime != SFBUnknownTime;
}

#endif /* __OBJC__ */
