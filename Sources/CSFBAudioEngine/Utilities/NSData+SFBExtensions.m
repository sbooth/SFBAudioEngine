//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "NSData+SFBExtensions.h"

@implementation NSData (SFBID3v2Methods)

- (BOOL)startsWithID3v2Header
{
	if(self.length < 10)
		return NO;

	/*
	 An ID3v2 tag can be detected with the following pattern:
	 $49 44 33 yy yy xx zz zz zz zz
	 Where yy is less than $FF, xx is the 'flags' byte and zz is less than
	 $80.
	 */

	const uint8_t *bytes = self.bytes;
	if(bytes[0] != 0x49 || bytes[1] != 0x44 || bytes[2] != 0x33)
		return NO;
	if(bytes[3] >= 0xff || bytes[4] >= 0xff)
		return NO;
	if(bytes[5] & 0xf)
		return NO;
	if(bytes[6] >= 0x80 || bytes[7] >= 0x80 || bytes[8] >= 0x80 || bytes[9] >= 0x80)
		return NO;
	return YES;
}

@end

@implementation NSData (SFBContentTypeMethods)

- (BOOL)isAIFFHeader
{
	if(self.length < 12)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "FORM", 4) && (!memcmp(bytes + 8, "AIFF", 4) || !memcmp(bytes + 8, "AIFC", 4));
}

- (BOOL)isCAFHeader
{
	return self.length >= 4 && !memcmp(self.bytes, "caff", 4);
}

- (BOOL)isDSDIFFHeader
{
	if(self.length < 16)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "FRM8", 4) && !memcmp(bytes + 12, "DSD ", 4);
}

- (BOOL)isDSFHeader
{
	if(self.length < 32)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "DSD ", 4) && !memcmp(bytes + 28, "fmt ", 4);
}

- (BOOL)isFLACHeader
{
	return self.length >= 4 && !memcmp(self.bytes, "fLaC", 4);
}

- (BOOL)isMonkeysAudioHeader
{
	return self.length >= 4 && !memcmp(self.bytes, "MAC ", 4);
}

- (BOOL)isMP3Header
{
	if(self.length < 3)
		return NO;

	const uint8_t *bytes = self.bytes;

	// Frame sync
	if(bytes[0] != 0xff || (bytes[1] & 0xe0) != 0xe0)
		return NO;
	// MPEG audio version ID
	else if((bytes[1] & 0x18) == 0x08)
		return NO;
	// Layer description
	else if((bytes[1] & 0x06) == 0)
		return NO;
	// Protection bit
	// Bitrate index
	else if((bytes[2] & 0xf0) == 0xf0)
		return NO;
	// Sampling rate frequency index
	else if((bytes[2] & 0x0c) == 0x0c)
		return NO;
	// Remainder of header bits ignored
	else
		return YES;
}

- (BOOL)isMPEG4Header
{
	return self.length >= 8 && !memcmp((const uint8_t *)self.bytes + 4, "ftyp", 4);
}

- (BOOL)isMusepackHeader
{
	if(self.length < 4)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "MPCK", 4) || !memcmp(bytes, "MP+", 3);
}

- (BOOL)isOggFLACHeader
{
	if(self.length < 33)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28, "\x7f""FLAC", 5);
}

- (BOOL)isOggOpusHeader
{
	if(self.length < 36)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28, "OpusHead", 8);
}

- (BOOL)isOggSpeexHeader
{
	if(self.length < 36)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28, "Speex   ", 8);
}

- (BOOL)isOggVorbisHeader
{
	if(self.length < 35)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28, "\x01vorbis", 7);
}

- (BOOL)isShortenHeader
{
	return self.length >= 4 && !memcmp(self.bytes, "ajkg", 4);
}

- (BOOL)isTrueAudioHeader
{
	return self.length >= 4 && !memcmp(self.bytes, "TTA1", 4);
}

- (BOOL)isWAVEHeader
{
	if(self.length < 12)
		return NO;
	const uint8_t *bytes = self.bytes;
	return !memcmp(bytes, "RIFF", 4) && !memcmp(bytes + 8, "WAVE", 4);
}

- (BOOL)isWavPackHeader
{
	return self.length >= 4 && !memcmp(self.bytes, "wvpk", 4);
}

@end
