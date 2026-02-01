//
// SPDX-FileCopyrightText: 2014 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBASBDFormatDescription.h"

#include <AudioToolbox/AudioFormat.h>

#include <libkern/OSByteOrder.h>

/// Common PCM audio formats.
enum CommonPCMFormat {
    /// Not a common PCM format.
    none,
    /// Native-endian float.
    float32,
    /// Native-endian double.
    float64,
    /// Native-endian int16_t.
    int16,
    /// Native-endian int32_t.
    int32,
};

/// Returns a descriptive format name for formatID or NULL if unknown.
static NSString *_Nullable formatIDName(AudioFormatID formatID) {
    switch (formatID) {
    case kAudioFormatLinearPCM:
        return @"Linear PCM";
    case kAudioFormatAC3:
        return @"AC-3";
    case kAudioFormat60958AC3:
        return @"AC-3 over IEC 60958";
    case kAudioFormatAppleIMA4:
        return @"IMA 4:1 ADPCM";
    case kAudioFormatMPEG4AAC:
        return @"MPEG-4 Low Complexity AAC";
    case kAudioFormatMPEG4CELP:
        return @"MPEG-4 CELP";
    case kAudioFormatMPEG4HVXC:
        return @"MPEG-4 HVXC";
    case kAudioFormatMPEG4TwinVQ:
        return @"MPEG-4 TwinVQ";
    case kAudioFormatMACE3:
        return @"MACE 3:1";
    case kAudioFormatMACE6:
        return @"MACE 6:1";
    case kAudioFormatULaw:
        return @"µ-law 2:1";
    case kAudioFormatALaw:
        return @"A-law 2:1";
    case kAudioFormatQDesign:
        return @"QDesign music";
    case kAudioFormatQDesign2:
        return @"QDesign2 music";
    case kAudioFormatQUALCOMM:
        return @"QUALCOMM PureVoice";
    case kAudioFormatMPEGLayer1:
        return @"MPEG-1/2 Layer I";
    case kAudioFormatMPEGLayer2:
        return @"MPEG-1/2 Layer II";
    case kAudioFormatMPEGLayer3:
        return @"MPEG-1/2 Layer III";
    case kAudioFormatTimeCode:
        return @"Stream of IOAudioTimeStamps";
    case kAudioFormatMIDIStream:
        return @"Stream of MIDIPacketLists";
    case kAudioFormatParameterValueStream:
        return @"Float32 side-chain";
    case kAudioFormatAppleLossless:
        return @"Apple Lossless";
    case kAudioFormatMPEG4AAC_HE:
        return @"MPEG-4 High Efficiency AAC";
    case kAudioFormatMPEG4AAC_LD:
        return @"MPEG-4 AAC Low Delay";
    case kAudioFormatMPEG4AAC_ELD:
        return @"MPEG-4 AAC Enhanced Low Delay";
    case kAudioFormatMPEG4AAC_ELD_SBR:
        return @"MPEG-4 AAC Enhanced Low Delay with SBR extension";
    case kAudioFormatMPEG4AAC_ELD_V2:
        return @"MPEG-4 AAC Enhanced Low Delay Version 2";
    case kAudioFormatMPEG4AAC_HE_V2:
        return @"MPEG-4 High Efficiency AAC Version 2";
    case kAudioFormatMPEG4AAC_Spatial:
        return @"MPEG-4 Spatial Audio";
    case kAudioFormatMPEGD_USAC:
        return @"MPEG-D Unified Speech and Audio Coding";
    case kAudioFormatAMR:
        return @"AMR Narrow Band";
    case kAudioFormatAMR_WB:
        return @"AMR Wide Band";
    case kAudioFormatAudible:
        return @"Audible";
    case kAudioFormatiLBC:
        return @"iLBC narrow band";
    case kAudioFormatDVIIntelIMA:
        return @"DVI/Intel IMA ADPCM";
    case kAudioFormatMicrosoftGSM:
        return @"Microsoft GSM 6.10";
    case kAudioFormatAES3:
        return @"AES3-2003";
    case kAudioFormatEnhancedAC3:
        return @"Enhanced AC-3";
    case kAudioFormatFLAC:
        return @"Free Lossless Audio Codec";
    case kAudioFormatOpus:
        return @"Opus";
    case kAudioFormatAPAC:
        return @"Apple Positional Audio Codec";
    default:
        return nil;
    }
}

/// Returns true if c is a printable ASCII character.
static bool isPrintableASCII(unsigned char c) {
    return c > 0x1F && c < 0x7F;
}

/// Creates a string representation of a four-character code.
static NSString *_Nullable fourCharCodeString(UInt32 fourcc) {
    union {
        UInt32 ui32;
        unsigned char str[4];
    } u;
    u.ui32 = OSSwapHostToBigInt32(fourcc);

    if (isPrintableASCII(u.str[0]) && isPrintableASCII(u.str[1]) && isPrintableASCII(u.str[2]) &&
        isPrintableASCII(u.str[3])) {
        return [NSString stringWithFormat:@"'%.4s'", u.str];
    }
    return [NSString stringWithFormat:@"0x%.02x%.02x%.02x%.02x", u.str[0], u.str[1], u.str[2], u.str[3]];
}

/// Returns the common PCM format described by an AudioStreamBasicDescription structure.
static enum CommonPCMFormat identifyCommonPCMFormat(const AudioStreamBasicDescription *_Nonnull streamDescription) {
    if (!streamDescription) {
        return none;
    }

    if (streamDescription->mFramesPerPacket != 1 ||
        streamDescription->mBytesPerFrame != streamDescription->mBytesPerPacket ||
        streamDescription->mChannelsPerFrame == 0) {
        return none;
    }

    // Exclude non-PCM, non-native endian, non-implicitly packed formats
    if (streamDescription->mFormatID != kAudioFormatLinearPCM ||
        (streamDescription->mFormatFlags & kAudioFormatFlagIsBigEndian) != kAudioFormatFlagsNativeEndian ||
        ((streamDescription->mBitsPerChannel / 8) *
         (((streamDescription->mFormatFlags & kAudioFormatFlagIsNonInterleaved) == 0)
                ? streamDescription->mChannelsPerFrame
                : 1)) != streamDescription->mBytesPerFrame) {
        return none;
    }

    if ((streamDescription->mFormatFlags & kAudioFormatFlagIsSignedInteger) == kAudioFormatFlagIsSignedInteger) {
        // Disqualify fixed point
        if ((streamDescription->mFormatFlags & kAudioFormatFlagIsFloat) == 0 &&
            ((streamDescription->mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask) >>
             kLinearPCMFormatFlagsSampleFractionShift) > 0) {
            return none;
        }

        if (streamDescription->mBitsPerChannel == 16) {
            return int16;
        }
        if (streamDescription->mBitsPerChannel == 32) {
            return int32;
        }
    } else if ((streamDescription->mFormatFlags & kAudioFormatFlagIsFloat) == kAudioFormatFlagIsFloat) {
        if (streamDescription->mBitsPerChannel == 32) {
            return float32;
        }
        if (streamDescription->mBitsPerChannel == 64) {
            return float64;
        }
    }

    return none;
}

NSString *SFBASBDFormatDescription(const AudioStreamBasicDescription *streamDescription) {
    if (!streamDescription) {
        return nil;
    }

    // Channels and sample rate
    NSMutableString *result = [NSMutableString
          stringWithFormat:@"%u ch @ %g Hz, ", streamDescription->mChannelsPerFrame, streamDescription->mSampleRate];

    // Shorter description for common formats
    enum CommonPCMFormat commonPCMFormat = identifyCommonPCMFormat(streamDescription);
    if (commonPCMFormat != none) {
        switch (commonPCMFormat) {
        case int16:
            [result appendString:@"Int16, "];
            break;
        case int32:
            [result appendString:@"Int32, "];
            break;
        case float32:
            [result appendString:@"Float32, "];
            break;
        case float64:
            [result appendString:@"Float64, "];
            break;
        default:
            [result appendString:@"_ PCM, "];
            break;
        }

        if ((streamDescription->mFormatFlags & kAudioFormatFlagIsNonInterleaved) == kAudioFormatFlagIsNonInterleaved) {
            [result appendString:@"deinterleaved"];
        } else {
            [result appendString:@"interleaved"];
        }

        return result;
    }

    if (streamDescription->mFormatID == kAudioFormatLinearPCM) {
        // Bit depth
        const UInt32 fractionalBits = (streamDescription->mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask) >>
                                      kLinearPCMFormatFlagsSampleFractionShift;
        if (fractionalBits > 0) {
            [result appendFormat:@"%d.%d-bit", streamDescription->mBitsPerChannel - fractionalBits, fractionalBits];
        } else {
            [result appendFormat:@"%d-bit", streamDescription->mBitsPerChannel];
        }

        const UInt32 interleavedChannelCount =
              ((streamDescription->mFormatFlags & kAudioFormatFlagIsNonInterleaved) == 0)
                    ? streamDescription->mChannelsPerFrame
                    : 1;
        const UInt32 sampleWordSize =
              (interleavedChannelCount == 0 || streamDescription->mBytesPerFrame % interleavedChannelCount != 0)
                    ? 0
                    : streamDescription->mBytesPerFrame / interleavedChannelCount;

        // Endianness
        if (sampleWordSize > 1) {
            if ((streamDescription->mFormatFlags & kAudioFormatFlagIsBigEndian) == kAudioFormatFlagIsBigEndian) {
                [result appendString:@" big-endian"];
            } else {
                [result appendString:@" little-endian"];
            }
        }

        // Sign
        // Integer or floating
        if ((streamDescription->mFormatFlags & kAudioFormatFlagIsFloat) == 0) {
            if ((streamDescription->mFormatFlags & kAudioFormatFlagIsSignedInteger) ==
                kAudioFormatFlagIsSignedInteger) {
                [result appendString:@" signed integer"];
            } else {
                [result appendString:@" unsigned integer"];
            }
        } else {
            [result appendString:@" float"];
        }

        // Packedness and alignment
        if (sampleWordSize > 0) {
            // Implicitly packed
            if (((streamDescription->mBitsPerChannel / 8) * interleavedChannelCount) ==
                streamDescription->mBytesPerFrame) {
                [result appendString:@", packed"];
            }
            // Unaligned
            else if ((sampleWordSize << 3) != streamDescription->mBitsPerChannel ||
                     (streamDescription->mBitsPerChannel & 7) != 0) {
                if ((streamDescription->mFormatFlags & kAudioFormatFlagIsAlignedHigh) ==
                    kAudioFormatFlagIsAlignedHigh) {
                    [result appendString:@" high-aligned"];
                } else {
                    [result appendString:@" low-aligned"];
                }
            }

            [result appendFormat:@" in %d bytes", sampleWordSize];
        }

        if ((streamDescription->mFormatFlags & kAudioFormatFlagIsNonInterleaved) == kAudioFormatFlagIsNonInterleaved) {
            [result appendString:@", deinterleaved"];
        }
    } else if (streamDescription->mFormatID == kAudioFormatAppleLossless ||
               streamDescription->mFormatID == kAudioFormatFLAC) {
        NSString *formatIDString = formatIDName(streamDescription->mFormatID);
        if (formatIDString) {
            [result appendString:formatIDString];
        } else {
            NSString *fourCC = fourCharCodeString(streamDescription->mFormatID);
            if (fourCC) {
                [result appendString:fourCC];
            } else {
                [result appendFormat:@"0x%.08x", streamDescription->mFormatID];
            }
        }
        [result appendString:@", "];

        UInt32 sourceBitDepth = 0;
        switch (streamDescription->mFormatFlags) {
        case kAppleLosslessFormatFlag_16BitSourceData:
            sourceBitDepth = 16;
            break;
        case kAppleLosslessFormatFlag_20BitSourceData:
            sourceBitDepth = 20;
            break;
        case kAppleLosslessFormatFlag_24BitSourceData:
            sourceBitDepth = 24;
            break;
        case kAppleLosslessFormatFlag_32BitSourceData:
            sourceBitDepth = 32;
            break;
        }

        if (sourceBitDepth != 0) {
            [result appendFormat:@"from %d-bit source, ", sourceBitDepth];
        } else {
            [result appendString:@"from UNKNOWN source bit depth, "];
        }

        [result appendFormat:@"%d frames/packet", streamDescription->mFramesPerPacket];
    } else {
        NSString *formatIDString = formatIDName(streamDescription->mFormatID);
        if (formatIDString) {
            [result appendString:formatIDString];
        } else {
            NSString *fourCC = fourCharCodeString(streamDescription->mFormatID);
            if (fourCC) {
                [result appendString:fourCC];
            } else {
                [result appendFormat:@"0x%.08x", streamDescription->mFormatID];
            }
        }

        // Format flags
        if (streamDescription->mFormatFlags != 0) {
            [result appendFormat:@" (%#x)", streamDescription->mFormatFlags];
        }

        [result appendFormat:@", %u bits/channel, %u bytes/packet, %u frames/packet, %u bytes/frame",
                             streamDescription->mBitsPerChannel, streamDescription->mBytesPerPacket,
                             streamDescription->mFramesPerPacket, streamDescription->mBytesPerFrame];
    }

    return result;
}
