//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// MARK: - ID3v2

@interface NSData (SFBID3v2Methods)
/// Returns `YES` if `self` starts with an ID3v2 tag header
- (BOOL)isID3v2Header;
@end

// MARK: - Content Type Detection

@interface NSData (SFBContentTypeMethods)
/// Returns `YES` if `self` starts with an AIFF or AIFC header
- (BOOL)isAIFFHeader;

/// Returns `YES` if `self` starts with a CAF header
- (BOOL)isCAFHeader;

/// Returns `YES` if `self` starts with a DSDIFF header
- (BOOL)isDSDIFFHeader;

/// Returns `YES` if `self` starts with a DSF header
- (BOOL)isDSFHeader;

/// Returns `YES` if `self` starts with a FLAC header
- (BOOL)isFLACHeader;

/// Returns `YES` if `self` starts with a Monkey's Audio header
- (BOOL)isMonkeysAudioHeader;

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
