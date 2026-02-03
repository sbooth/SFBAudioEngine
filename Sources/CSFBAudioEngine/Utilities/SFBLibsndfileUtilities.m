//
// SPDX-FileCopyrightText: 2024 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBLibsndfileUtilities.h"

#import "SFBAudioEngineTypes.h"

#import <sndfile/sndfile.h>

void FillASBDWithSndfileFormat(AudioStreamBasicDescription *asbd, int format) {
    NSCParameterAssert(asbd != NULL);

    int majorFormat = format & SF_FORMAT_TYPEMASK;
    int subtype = format & SF_FORMAT_SUBMASK;

    switch (subtype) {
    case SF_FORMAT_PCM_U8:
        asbd->mFormatID = kAudioFormatLinearPCM;
        asbd->mBitsPerChannel = 8;
        break;

    case SF_FORMAT_PCM_S8:
        if (majorFormat == SF_FORMAT_FLAC) {
            asbd->mFormatID = kAudioFormatFLAC;
        } else {
            asbd->mFormatID = kAudioFormatLinearPCM;
            asbd->mFormatFlags = kAudioFormatFlagIsSignedInteger;
            asbd->mBitsPerChannel = 8;
        }
        break;

    case SF_FORMAT_PCM_16:
        if (majorFormat == SF_FORMAT_FLAC) {
            asbd->mFormatID = kAudioFormatFLAC;
            asbd->mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
        } else {
            asbd->mFormatID = kAudioFormatLinearPCM;
            asbd->mFormatFlags = kAudioFormatFlagIsSignedInteger;
            asbd->mBitsPerChannel = 16;
        }
        break;

    case SF_FORMAT_PCM_24:
        if (majorFormat == SF_FORMAT_FLAC) {
            asbd->mFormatID = kAudioFormatFLAC;
            asbd->mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;
        } else {
            asbd->mFormatID = kAudioFormatLinearPCM;
            asbd->mFormatFlags = kAudioFormatFlagIsSignedInteger;
            asbd->mBitsPerChannel = 24;
        }
        break;

    case SF_FORMAT_PCM_32:
        asbd->mFormatID = kAudioFormatLinearPCM;
        asbd->mFormatFlags = kAudioFormatFlagIsSignedInteger;
        asbd->mBitsPerChannel = 32;
        break;

    case SF_FORMAT_FLOAT:
        //        asbd->mFormatID = kAudioFormatLinearPCM;
        asbd->mFormatFlags = kAudioFormatFlagIsFloat;
        asbd->mBitsPerChannel = 32;
        break;

    case SF_FORMAT_DOUBLE:
        //        asbd->mFormatID = kAudioFormatLinearPCM;
        asbd->mFormatFlags = kAudioFormatFlagIsFloat;
        asbd->mBitsPerChannel = 64;
        break;

    case SF_FORMAT_VORBIS:
        asbd->mFormatID = kSFBAudioFormatVorbis;
        break;

    case SF_FORMAT_OPUS:
        asbd->mFormatID = kAudioFormatOpus;
        break;

    case SF_FORMAT_ALAC_16:
        asbd->mFormatID = kAudioFormatAppleLossless;
        asbd->mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
        break;

    case SF_FORMAT_ALAC_20:
        asbd->mFormatID = kAudioFormatAppleLossless;
        asbd->mFormatFlags = kAppleLosslessFormatFlag_20BitSourceData;
        break;

    case SF_FORMAT_ALAC_24:
        asbd->mFormatID = kAudioFormatAppleLossless;
        asbd->mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;
        break;

    case SF_FORMAT_ALAC_32:
        asbd->mFormatID = kAudioFormatAppleLossless;
        asbd->mFormatFlags = kAppleLosslessFormatFlag_32BitSourceData;
        break;

    case SF_FORMAT_ULAW:
        asbd->mFormatID = kAudioFormatULaw;
        asbd->mBitsPerChannel = 8;
        break;

    case SF_FORMAT_ALAW:
        asbd->mFormatID = kAudioFormatALaw;
        asbd->mBitsPerChannel = 8;
        break;
    }
}
