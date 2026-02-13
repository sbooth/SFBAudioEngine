//
// SPDX-FileCopyrightText: 2014 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBACLDescription.h"

#include <AudioToolbox/AudioFormat.h>

/// Returns the string representation of an AudioChannelLayoutTag.
NSString *_Nullable channelLayoutTagName(AudioChannelLayoutTag layoutTag) {
    switch (layoutTag) {
    case kAudioChannelLayoutTag_UseChannelDescriptions:
        return @"Use Channel Descriptions";
    case kAudioChannelLayoutTag_UseChannelBitmap:
        return @"Use Channel Bitmap";

    case kAudioChannelLayoutTag_Mono:
        return @"Mono";
    case kAudioChannelLayoutTag_Stereo:
        return @"Stereo";
    case kAudioChannelLayoutTag_StereoHeadphones:
        return @"Stereo Headphones";
    case kAudioChannelLayoutTag_MatrixStereo:
        return @"Matrix Stereo";
    case kAudioChannelLayoutTag_MidSide:
        return @"Mid-Side";
    case kAudioChannelLayoutTag_XY:
        return @"XY";
    case kAudioChannelLayoutTag_Binaural:
        return @"Binaural";
    case kAudioChannelLayoutTag_Ambisonic_B_Format:
        return @"Ambisonic B-format";
    case kAudioChannelLayoutTag_Quadraphonic:
        return @"Quadraphonic";
    case kAudioChannelLayoutTag_Pentagonal:
        return @"Pentagonal";
    case kAudioChannelLayoutTag_Hexagonal:
        return @"Hexagonal";
    case kAudioChannelLayoutTag_Octagonal:
        return @"Octagonal";
    case kAudioChannelLayoutTag_Cube:
        return @"Cube";
    case kAudioChannelLayoutTag_MPEG_3_0_A:
        return @"MPEG 3.0 A";
    case kAudioChannelLayoutTag_MPEG_3_0_B:
        return @"MPEG 3.0 B";
    case kAudioChannelLayoutTag_MPEG_4_0_A:
        return @"MPEG 4.0 A";
    case kAudioChannelLayoutTag_MPEG_4_0_B:
        return @"MPEG 4.0 B";
    case kAudioChannelLayoutTag_MPEG_5_0_A:
        return @"MPEG 5.0 A";
    case kAudioChannelLayoutTag_MPEG_5_0_B:
        return @"MPEG 5.0 B";
    case kAudioChannelLayoutTag_MPEG_5_0_C:
        return @"MPEG 5.0 C";
    case kAudioChannelLayoutTag_MPEG_5_0_D:
        return @"MPEG 5.0 D";
    case kAudioChannelLayoutTag_MPEG_5_1_A:
        return @"MPEG 5.1 A";
    case kAudioChannelLayoutTag_MPEG_5_1_B:
        return @"MPEG 5.1 B";
    case kAudioChannelLayoutTag_MPEG_5_1_C:
        return @"MPEG 5.1 C";
    case kAudioChannelLayoutTag_MPEG_5_1_D:
        return @"MPEG 5.1 D";
    case kAudioChannelLayoutTag_MPEG_6_1_A:
        return @"MPEG 6.1 A";
    case kAudioChannelLayoutTag_MPEG_7_1_A:
        return @"MPEG 7.1 A";
    case kAudioChannelLayoutTag_MPEG_7_1_B:
        return @"MPEG 7.1 B";
    case kAudioChannelLayoutTag_MPEG_7_1_C:
        return @"MPEG 7.1 C";
    case kAudioChannelLayoutTag_Emagic_Default_7_1:
        return @"Emagic Default 7.1";
    case kAudioChannelLayoutTag_SMPTE_DTV:
        return @"SMPTE DTV";
    case kAudioChannelLayoutTag_ITU_2_1:
        return @"ITU 2.1";
    case kAudioChannelLayoutTag_ITU_2_2:
        return @"ITU 2.2";
    case kAudioChannelLayoutTag_DVD_4:
        return @"DVD 4";
    case kAudioChannelLayoutTag_DVD_5:
        return @"DVD 5";
    case kAudioChannelLayoutTag_DVD_6:
        return @"DVD 6";
    case kAudioChannelLayoutTag_DVD_10:
        return @"DVD 10";
    case kAudioChannelLayoutTag_DVD_11:
        return @"DVD 11";
    case kAudioChannelLayoutTag_DVD_18:
        return @"DVD 18";
    case kAudioChannelLayoutTag_AudioUnit_6_0:
        return @"AudioUnit 6.0";
    case kAudioChannelLayoutTag_AudioUnit_7_0:
        return @"AudioUnit 7.0";
    case kAudioChannelLayoutTag_AudioUnit_7_0_Front:
        return @"AudioUnit 7.0 Front";
    case kAudioChannelLayoutTag_AAC_6_0:
        return @"AAC 6.0";
    case kAudioChannelLayoutTag_AAC_6_1:
        return @"AAC 6.1";
    case kAudioChannelLayoutTag_AAC_7_0:
        return @"AAC 7.0";
    case kAudioChannelLayoutTag_AAC_7_1_B:
        return @"AAC 7.1 B";
    case kAudioChannelLayoutTag_AAC_7_1_C:
        return @"AAC 7.1 C";
    case kAudioChannelLayoutTag_AAC_Octagonal:
        return @"AAC Octagonal";
    case kAudioChannelLayoutTag_TMH_10_2_std:
        return @"TMH 10.2 standard";
    case kAudioChannelLayoutTag_TMH_10_2_full:
        return @"TMH 10.2 full";
    case kAudioChannelLayoutTag_AC3_1_0_1:
        return @"AC-3 1.0.1";
    case kAudioChannelLayoutTag_AC3_3_0:
        return @"AC-3 3.0";
    case kAudioChannelLayoutTag_AC3_3_1:
        return @"AC-3 3.1";
    case kAudioChannelLayoutTag_AC3_3_0_1:
        return @"AC-3 3.0.1";
    case kAudioChannelLayoutTag_AC3_2_1_1:
        return @"AC-3 2.1.1";
    case kAudioChannelLayoutTag_AC3_3_1_1:
        return @"AC-3 3.1.1";
    case kAudioChannelLayoutTag_EAC_6_0_A:
        return @"EAC 6.0 A";
    case kAudioChannelLayoutTag_EAC_7_0_A:
        return @"EAC 7.0 A";
    case kAudioChannelLayoutTag_EAC3_6_1_A:
        return @"E-AC-3 6.1 A";
    case kAudioChannelLayoutTag_EAC3_6_1_B:
        return @"E-AC-3 6.1 B";
    case kAudioChannelLayoutTag_EAC3_6_1_C:
        return @"E-AC-3 6.1 C";
    case kAudioChannelLayoutTag_EAC3_7_1_A:
        return @"E-AC-3 7.1 A";
    case kAudioChannelLayoutTag_EAC3_7_1_B:
        return @"E-AC-3 7.1 B";
    case kAudioChannelLayoutTag_EAC3_7_1_C:
        return @"E-AC-3 7.1 C";
    case kAudioChannelLayoutTag_EAC3_7_1_D:
        return @"E-AC-3 7.1 D";
    case kAudioChannelLayoutTag_EAC3_7_1_E:
        return @"E-AC-3 7.1 E";
    case kAudioChannelLayoutTag_EAC3_7_1_F:
        return @"E-AC-3 7.1 F";
    case kAudioChannelLayoutTag_EAC3_7_1_G:
        return @"E-AC-3 7.1 G";
    case kAudioChannelLayoutTag_EAC3_7_1_H:
        return @"E-AC-3 7.1 H";
    case kAudioChannelLayoutTag_DTS_3_1:
        return @"DTS 3.1";
    case kAudioChannelLayoutTag_DTS_4_1:
        return @"DTS 4.1";
    case kAudioChannelLayoutTag_DTS_6_0_A:
        return @"DTS 6.0 A";
    case kAudioChannelLayoutTag_DTS_6_0_B:
        return @"DTS 6.0 B";
    case kAudioChannelLayoutTag_DTS_6_0_C:
        return @"DTS 6.0 C";
    case kAudioChannelLayoutTag_DTS_6_1_A:
        return @"DTS 6.1 A";
    case kAudioChannelLayoutTag_DTS_6_1_B:
        return @"DTS 6.1 B";
    case kAudioChannelLayoutTag_DTS_6_1_C:
        return @"DTS 6.1 C";
    case kAudioChannelLayoutTag_DTS_7_0:
        return @"DTS 7.0";
    case kAudioChannelLayoutTag_DTS_7_1:
        return @"DTS 7.1";
    case kAudioChannelLayoutTag_DTS_8_0_A:
        return @"DTS 8.0 A";
    case kAudioChannelLayoutTag_DTS_8_0_B:
        return @"DTS 8.0 B";
    case kAudioChannelLayoutTag_DTS_8_1_A:
        return @"DTS 8.1 A";
    case kAudioChannelLayoutTag_DTS_8_1_B:
        return @"DTS 8.1 B";
    case kAudioChannelLayoutTag_DTS_6_1_D:
        return @"DTS 6.1 D";
    case kAudioChannelLayoutTag_WAVE_4_0_B:
        return @"WAVE 4.0 B";
    case kAudioChannelLayoutTag_WAVE_5_0_B:
        return @"WAVE 5.0 B";
    case kAudioChannelLayoutTag_WAVE_5_1_B:
        return @"WAVE 5.1 B";
    case kAudioChannelLayoutTag_WAVE_6_1:
        return @"WAVE 6.1";
    case kAudioChannelLayoutTag_WAVE_7_1:
        return @"WAVE 7.1";
    case kAudioChannelLayoutTag_Atmos_5_1_2:
        return @"Atmos 5.1.2";
    case kAudioChannelLayoutTag_Atmos_5_1_4:
        return @"Atmos 5.1.4";
    case kAudioChannelLayoutTag_Atmos_7_1_2:
        return @"Atmos 7.1.2";
    case kAudioChannelLayoutTag_Atmos_7_1_4:
        return @"Atmos 7.1.4";
    case kAudioChannelLayoutTag_Atmos_9_1_6:
        return @"Atmos 9.1.6";
    case kAudioChannelLayoutTag_Logic_4_0_C:
        return @"Logic 4.0 C";
    case kAudioChannelLayoutTag_Logic_6_0_B:
        return @"Logic 6.0 B";
    case kAudioChannelLayoutTag_Logic_6_1_B:
        return @"Logic 6.1 B";
    case kAudioChannelLayoutTag_Logic_6_1_D:
        return @"Logic 6.1 D";
    case kAudioChannelLayoutTag_Logic_7_1_B:
        return @"Logic 7.1 B";
    case kAudioChannelLayoutTag_Logic_Atmos_7_1_4_B:
        return @"Logic Atmos 7.1.4 B";
    case kAudioChannelLayoutTag_Logic_Atmos_7_1_6:
        return @"Logic Atmos 7.1.6";
    case kAudioChannelLayoutTag_CICP_13:
        return @"CICP 13";
    case kAudioChannelLayoutTag_CICP_14:
        return @"CICP 14";
    case kAudioChannelLayoutTag_CICP_15:
        return @"CICP 15";
    case kAudioChannelLayoutTag_CICP_16:
        return @"CICP 16";
    case kAudioChannelLayoutTag_CICP_17:
        return @"CICP 17";
    case kAudioChannelLayoutTag_CICP_18:
        return @"CICP 18";
    case kAudioChannelLayoutTag_CICP_19:
        return @"CICP 19";
    case kAudioChannelLayoutTag_CICP_20:
        return @"CICP 20";
    case kAudioChannelLayoutTag_Ogg_5_0:
        return @"Ogg 5.0";
    case kAudioChannelLayoutTag_Ogg_5_1:
        return @"Ogg 5.1";
    case kAudioChannelLayoutTag_Ogg_6_1:
        return @"Ogg 6.1";
    case kAudioChannelLayoutTag_Ogg_7_1:
        return @"Ogg 7.1";
    case kAudioChannelLayoutTag_MPEG_5_0_E:
        return @"MPEG 5.0 E";
    case kAudioChannelLayoutTag_MPEG_5_1_E:
        return @"MPEG 5.1 E";
    case kAudioChannelLayoutTag_MPEG_6_1_B:
        return @"MPEG 6.1 B";
    case kAudioChannelLayoutTag_MPEG_7_1_D:
        return @"MPEG 7.1 D";

    default:
        break;
    }

    if (layoutTag >= kAudioChannelLayoutTag_BeginReserved && layoutTag <= kAudioChannelLayoutTag_EndReserved) {
        return @"Reserved";
    }

    switch (layoutTag & 0xFFFF0000) {
    case kAudioChannelLayoutTag_HOA_ACN_SN3D:
        return @"HOA ACN SN3D";
    case kAudioChannelLayoutTag_HOA_ACN_N3D:
        return @"HOA ACN N3D";
    case kAudioChannelLayoutTag_DiscreteInOrder:
        return @"Discrete in Order";
    case kAudioChannelLayoutTag_Unknown:
        return @"Unknown";

    default:
        break;
    }

    return nil;
}

/// Returns the name of the channel for an AudioChannelLabel.
///
/// This is the value of kAudioFormatProperty_ChannelShortName or kAudioFormatProperty_ChannelName.
static NSString *_Nullable channelLabelName(AudioChannelLabel channelLabel, BOOL shortName) {
    AudioFormatPropertyID property =
            shortName ? kAudioFormatProperty_ChannelShortName : kAudioFormatProperty_ChannelName;
    CFStringRef channelName = NULL;
    UInt32 dataSize = sizeof channelName;
    OSStatus status = AudioFormatGetProperty(property, sizeof channelLabel, &channelLabel, &dataSize, &channelName);

    if (status != noErr) {
        return nil;
    }

    return (__bridge_transfer NSString *)channelName;
}

/// Returns the name of the channel layout described by an AudioChannelLayout structure.
///
/// This is the value of kAudioFormatProperty_ChannelLayoutName or kAudioFormatProperty_ChannelLayoutSimpleName.
static NSString *_Nullable channelLayoutName(const AudioChannelLayout *_Nullable channelLayout, BOOL simpleName) {
    if (!channelLayout) {
        return nil;
    }

    AudioFormatPropertyID property =
            simpleName ? kAudioFormatProperty_ChannelLayoutSimpleName : kAudioFormatProperty_ChannelLayoutName;
    UInt32 layoutSize = offsetof(AudioChannelLayout, mChannelDescriptions) +
                        (channelLayout->mNumberChannelDescriptions * sizeof(AudioChannelDescription));
    CFStringRef layoutName = NULL;
    UInt32 dataSize = sizeof layoutName;
    OSStatus status = AudioFormatGetProperty(property, layoutSize, channelLayout, &dataSize, &layoutName);

    if (status != noErr) {
        return nil;
    }

    return (__bridge_transfer NSString *)layoutName;
}

NSString *SFBACLDescription(const AudioChannelLayout *channelLayout) {
    if (!channelLayout) {
        return nil;
    }

    NSString *layoutName = channelLayoutName(channelLayout, false);

    if (channelLayout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions) {
        NSMutableString *result = [NSMutableString string];

        // kAudioFormatProperty_ChannelLayoutName returns '!fmt' for kAudioChannelLabel_UseCoordinates
        if (layoutName) {
            [result appendFormat:@"%u channel descriptions, %@", channelLayout->mNumberChannelDescriptions, layoutName];
            return result;
        }

        [result appendFormat:@"%u channel descriptions", channelLayout->mNumberChannelDescriptions];

        NSMutableArray *array = [NSMutableArray array];
        for (UInt32 i = 0; i < channelLayout->mNumberChannelDescriptions; ++i) {
            const AudioChannelDescription *desc = channelLayout->mChannelDescriptions + i;

            if (desc->mChannelLabel == kAudioChannelLabel_UseCoordinates) {
                NSString *formatString = nil;
                if (desc->mChannelFlags & kAudioChannelFlags_RectangularCoordinates) {
                    formatString = @"[x: %g, y: %g, z: %g%s]";
                } else if (desc->mChannelFlags & kAudioChannelFlags_SphericalCoordinates) {
                    formatString = @"[r: %g, θ: %g, φ: %g%s]";
                } else {
                    formatString = @"[?! %g, %g, %g%s]";
                }

                NSString *coordinateString =
                        [NSString stringWithFormat:formatString, desc->mCoordinates[0], desc->mCoordinates[1],
                                                   desc->mCoordinates[2],
                                                   (desc->mChannelFlags & kAudioChannelFlags_Meters) ? " m" : ""];

                [array addObject:coordinateString];
            } else {
                NSString *channelName = channelLabelName(desc->mChannelLabel, true);
                if (channelName) {
                    [array addObject:channelName];
                } else {
                    [array addObject:@"_"];
                }
            }
        }

        NSString *channelNamesString = [array componentsJoinedByString:@" "];
        if (channelNamesString) {
            [result appendFormat:@", %@", channelNamesString];
        }

        return result;
    }

    if (channelLayout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap) {
        NSMutableString *result =
                [NSMutableString stringWithFormat:@"Bitmap %#x (%u ch)", channelLayout->mChannelBitmap,
                                                  __builtin_popcount(channelLayout->mChannelBitmap)];
        if (layoutName) {
            [result appendFormat:@", %@", layoutName];
        }
        return result;
    }

    NSMutableString *result = [NSMutableString
            stringWithFormat:@"%@ (0x%x, %u ch)", channelLayoutTagName(channelLayout->mChannelLayoutTag),
                             channelLayout->mChannelLayoutTag, channelLayout->mChannelLayoutTag & 0xffff];
    if (layoutName) {
        [result appendFormat:@", %@", layoutName];
    }
    return result;
}
