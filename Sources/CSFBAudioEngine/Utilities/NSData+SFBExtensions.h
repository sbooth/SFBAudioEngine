//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// MARK: - ID3v2

/// The size of an ID3v2 tag header, in bytes
extern const NSUInteger SFBID3v2HeaderSize NS_SWIFT_NAME(id3v2HeaderSize);
/// The size of an ID3v2 tag footer, in bytes
extern const NSUInteger SFBID3v2FooterSize NS_SWIFT_NAME(id3v2FooterSize);

@interface NSData (SFBID3v2Methods)
/// Returns `YES` if `self` starts with an ID3v2 tag header
- (BOOL)isID3v2Header;
/// If `self` is an ID3v2 tag header, returns the total size of the ID3v2 tag including the header and footer, if present
- (NSUInteger)id3v2TagTotalSize;
@end

// MARK: - Content Type Detection

/// The minimum size for AIFF detection, in bytes
extern const NSUInteger SFBAIFFDetectionSize NS_SWIFT_NAME(aiffDetectionSize);
/// The minimum size for APE detection, in bytes
extern const NSUInteger SFBAPEDetectionSize NS_SWIFT_NAME(apeDetectionSize);
/// The minimum size for CAF detection, in bytes
extern const NSUInteger SFBCAFDetectionSize NS_SWIFT_NAME(cafDetectionSize);
/// The minimum size for DSDIFF detection, in bytes
extern const NSUInteger SFBDSDIFFDetectionSize NS_SWIFT_NAME(dsdiffDetectionSize);
/// The minimum size for DSF detection, in bytes
extern const NSUInteger SFBDSFDetectionSize NS_SWIFT_NAME(dsfDetectionSize);
/// The minimum size for FLAC detection, in bytes
extern const NSUInteger SFBFLACDetectionSize NS_SWIFT_NAME(flacDetectionSize);
/// The minimum size for MP3 detection, in bytes
extern const NSUInteger SFBMP3DetectionSize NS_SWIFT_NAME(mp3DetectionSize);
/// The minimum size for MPEG-4 detection, in bytes
extern const NSUInteger SFBMPEG4DetectionSize NS_SWIFT_NAME(mpeg4DetectionSize);
/// The minimum size for Musepack detection, in bytes
extern const NSUInteger SFBMusepackDetectionSize NS_SWIFT_NAME(musepackDetectionSize);
/// The minimum size for Ogg FLAC detection, in bytes
extern const NSUInteger SFBOggFLACDetectionSize NS_SWIFT_NAME(oggFLACDetectionSize);
/// The minimum size for Ogg Opus detection, in bytes
extern const NSUInteger SFBOggOpusDetectionSize NS_SWIFT_NAME(oggOpusDetectionSize);
/// The minimum size for Ogg Speex detection, in bytes
extern const NSUInteger SFBOggSpeexDetectionSize NS_SWIFT_NAME(oggSpeexDetectionSize);
/// The minimum size for Ogg Vorbis detection, in bytes
extern const NSUInteger SFBOggVorbisDetectionSize NS_SWIFT_NAME(oggVorbisDetectionSize);
/// The minimum size for Shorten detection, in bytes
extern const NSUInteger SFBShortenDetectionSize NS_SWIFT_NAME(shortenDetectionSize);
/// The minimum size for True Audio detection, in bytes
extern const NSUInteger SFBTrueAudioDetectionSize NS_SWIFT_NAME(trueAudioDetectionSize);
/// The minimum size for WAVE detection, in bytes
extern const NSUInteger SFBWAVEDetectionSize NS_SWIFT_NAME(waveDetectionSize);
/// The minimum size for WavPack detection, in bytes
extern const NSUInteger SFBWavPackDetectionSize NS_SWIFT_NAME(wavPackDetectionSize);


@interface NSData (SFBContentTypeMethods)
/// Returns `YES` if `self` starts with an AIFF or AIFC header
- (BOOL)isAIFFHeader;

/// Returns `YES` if `self` starts with a Monkey's Audio header
- (BOOL)isAPEHeader;

/// Returns `YES` if `self` starts with a CAF header
- (BOOL)isCAFHeader;

/// Returns `YES` if `self` starts with a DSDIFF header
- (BOOL)isDSDIFFHeader;

/// Returns `YES` if `self` starts with a DSF header
- (BOOL)isDSFHeader;

/// Returns `YES` if `self` starts with a FLAC header
- (BOOL)isFLACHeader;

/// Returns `YES` if `self` starts with an MP3 header
- (BOOL)isMP3Header;

/// Returns `YES` if `self` starts with an MPEG-4 header
- (BOOL)isMPEG4Header;

/// Returns `YES` if `self` starts with a Musepack header
- (BOOL)isMusepackHeader;

/// Returns `YES` if `self` starts with an Ogg FLAC header
- (BOOL)isOggFLACHeader;

/// Returns `YES` if `self` starts with an Ogg Opus header
- (BOOL)isOggOpusHeader;

/// Returns `YES` if `self` starts with an Ogg Speex header
- (BOOL)isOggSpeexHeader;

/// Returns `YES` if `self` starts with an Ogg Vorbis header
- (BOOL)isOggVorbisHeader;

/// Returns `YES` if `self` starts with a Shorten header
- (BOOL)isShortenHeader;

/// Returns `YES` if `self` starts with a True Audio header
- (BOOL)isTrueAudioHeader;

/// Returns `YES` if `self` starts with a WAVE header
- (BOOL)isWAVEHeader;

/// Returns `YES` if `self` starts with a WavPack header
- (BOOL)isWavPackHeader;
@end

NS_ASSUME_NONNULL_END
