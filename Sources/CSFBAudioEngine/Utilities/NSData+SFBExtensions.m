//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "NSData+SFBExtensions.h"

const NSUInteger SFBID3v2HeaderSize = 10;
const NSUInteger SFBID3v2FooterSize = 10;

@implementation NSData (SFBID3v2Methods)

- (BOOL)isID3v2Header {
    if (self.length < SFBID3v2HeaderSize) {
        return NO;
    }

    /*
     An ID3v2 tag can be detected with the following pattern:
     $49 44 33 yy yy xx zz zz zz zz
     Where yy is less than $FF, xx is the 'flags' byte and zz is less than
     $80.
     */

    const unsigned char *bytes = self.bytes;
    if (bytes[0] != 0x49 || bytes[1] != 0x44 || bytes[2] != 0x33) {
        return NO;
    }
    if (bytes[3] >= 0xff || bytes[4] >= 0xff) {
        return NO;
    }
    if (bytes[5] & 0xf) {
        return NO;
    }
    if (bytes[6] >= 0x80 || bytes[7] >= 0x80 || bytes[8] >= 0x80 || bytes[9] >= 0x80) {
        return NO;
    }
    return YES;
}

- (NSUInteger)id3v2TagTotalSize {
    if (self.length < SFBID3v2HeaderSize) {
        return 0;
    }

    const unsigned char *bytes = self.bytes;

    unsigned char flags = bytes[5];
    uint32_t      size = (bytes[6] << 21) | (bytes[7] << 14) | (bytes[8] << 7) | bytes[9];

    return SFBID3v2HeaderSize + size + (flags & 0x10 ? SFBID3v2FooterSize : 0);
}

@end

const NSUInteger SFBAIFFDetectionSize = 12;
const NSUInteger SFBAPEDetectionSize = 4;
const NSUInteger SFBCAFDetectionSize = 4;
const NSUInteger SFBDSDIFFDetectionSize = 16;
const NSUInteger SFBDSFDetectionSize = 32;
const NSUInteger SFBFLACDetectionSize = 4;
const NSUInteger SFBMP3DetectionSize = 3;
const NSUInteger SFBMPEG4DetectionSize = 8;
const NSUInteger SFBMusepackDetectionSize = 4;
const NSUInteger SFBOggFLACDetectionSize = 33;
const NSUInteger SFBOggOpusDetectionSize = 36;
const NSUInteger SFBOggSpeexDetectionSize = 36;
const NSUInteger SFBOggVorbisDetectionSize = 35;
const NSUInteger SFBShortenDetectionSize = 4;
const NSUInteger SFBTrueAudioDetectionSize = 4;
const NSUInteger SFBWAVEDetectionSize = 12;
const NSUInteger SFBWavPackDetectionSize = 4;

@implementation NSData (SFBContentTypeMethods)

- (BOOL)isAIFFHeader {
    if (self.length < SFBAIFFDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "FORM", 4) && (!memcmp(bytes + 8, "AIFF", 4) || !memcmp(bytes + 8, "AIFC", 4));
}

- (BOOL)isAPEHeader {
    if (self.length < SFBAPEDetectionSize) {
        return NO;
    }
    return !memcmp(self.bytes, "MAC ", 4);
}

- (BOOL)isCAFHeader {
    if (self.length < SFBCAFDetectionSize) {
        return NO;
    }
    return !memcmp(self.bytes, "caff", 4);
}

- (BOOL)isDSDIFFHeader {
    if (self.length < SFBDSDIFFDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "FRM8", 4) && !memcmp(bytes + 12, "DSD ", 4);
}

- (BOOL)isDSFHeader {
    if (self.length < SFBDSFDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "DSD ", 4) && !memcmp(bytes + 28, "fmt ", 4);
}

- (BOOL)isFLACHeader {
    if (self.length < SFBFLACDetectionSize) {
        return NO;
    }
    return !memcmp(self.bytes, "fLaC", 4);
}

- (BOOL)isMP3Header {
    if (self.length < SFBMP3DetectionSize) {
        return NO;
    }

    const unsigned char *bytes = self.bytes;

    // Frame sync
    if (bytes[0] != 0xff || (bytes[1] & 0xe0) != 0xe0) {
        return NO;
    }
    // MPEG audio version ID
    if ((bytes[1] & 0x18) == 0x08) {
        return NO;
    }
    // Layer description
    if ((bytes[1] & 0x06) == 0) {
        return NO;
    }
    // Protection bit
    // Bitrate index
    if ((bytes[2] & 0xf0) == 0xf0) {
        return NO;
    }
    // Sampling rate frequency index
    if ((bytes[2] & 0x0c) == 0x0c) {
        return NO;
    }
    // Remainder of header bits ignored
    return YES;
}

- (BOOL)isMPEG4Header {
    if (self.length < SFBMPEG4DetectionSize) {
        return NO;
    }
    return !memcmp((const unsigned char *)self.bytes + 4, "ftyp", 4);
}

- (BOOL)isMusepackHeader {
    if (self.length < SFBMusepackDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "MPCK", 4) || !memcmp(bytes, "MP+", 3);
}

- (BOOL)isOggFLACHeader {
    if (self.length < SFBOggFLACDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28,
                                                  "\x7f"
                                                  "FLAC",
                                                  5);
}

- (BOOL)isOggOpusHeader {
    if (self.length < SFBOggOpusDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28, "OpusHead", 8);
}

- (BOOL)isOggSpeexHeader {
    if (self.length < SFBOggSpeexDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28, "Speex   ", 8);
}

- (BOOL)isOggVorbisHeader {
    if (self.length < SFBOggVorbisDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "OggS\0", 5) && !memcmp(bytes + 28, "\x01vorbis", 7);
}

- (BOOL)isShortenHeader {
    if (self.length < SFBShortenDetectionSize) {
        return NO;
    }
    return !memcmp(self.bytes, "ajkg", 4);
}

- (BOOL)isTrueAudioHeader {
    if (self.length < SFBTrueAudioDetectionSize) {
        return NO;
    }
    return !memcmp(self.bytes, "TTA1", 4);
}

- (BOOL)isWAVEHeader {
    if (self.length < SFBWAVEDetectionSize) {
        return NO;
    }
    const unsigned char *bytes = self.bytes;
    return !memcmp(bytes, "RIFF", 4) && !memcmp(bytes + 8, "WAVE", 4);
}

- (BOOL)isWavPackHeader {
    if (self.length < SFBWavPackDetectionSize) {
        return NO;
    }
    return !memcmp(self.bytes, "wvpk", 4);
}

@end
