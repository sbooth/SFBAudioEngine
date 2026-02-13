//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBLibsndfileEncoder.h"

#import "SFBLibsndfileUtilities.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>
#import <sndfile/sndfile.h>

#import <AudioToolbox/AudioToolbox.h>

#import <os/log.h>

SFBAudioEncoderName const SFBAudioEncoderNameLibsndfile = @"org.sbooth.AudioEngine.Encoder.Libsndfile";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileMajorFormat = @"Major Format";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileSubtype = @"Subtype";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileFileEndian = @"File Endian";

SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatWAV = @"WAV";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatAIFF =
        @"AIFF";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatAU = @"AU";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatRaw = @"Raw";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatPAF = @"PAF";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatSVX = @"SVX";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatNIST =
        @"NIST";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatVOC = @"VOC";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatIRCAM =
        @"IRCAM";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatW64 = @"W64";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatMAT4 =
        @"MAT4";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatMAT5 =
        @"MAT5";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatPVF = @"PVF";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatXI = @"XI";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatHTK = @"HTK";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatSDS = @"SDS";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatAVR = @"AVR";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatWAVEX =
        @"WAVEX";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatSD2 = @"SD2";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatFLAC =
        @"FLAC";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatCAF = @"CAF";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatWVE = @"WVE";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatOgg = @"Ogg";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatMPC2K =
        @"MPC2K";
SFBAudioEncodingSettingsValueLibsndfileMajorFormat const SFBAudioEncodingSettingsValueLibsndfileMajorFormatRF64 =
        @"RF64";

SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_S8 = @"PCM_S8";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_16 = @"PCM_16";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_24 = @"PCM_24";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_32 = @"PCM_32";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_U8 = @"PCM_U8";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeFloat = @"Float";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDouble = @"Double";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeµLAW = @"µ-law";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAW = @"A-law";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeIMA_ADPCM =
        @"IMA_ADPCM";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeMS_ADPCM =
        @"MS_ADPCM";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeGSM610 = @"GSM610";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeVOX_ADPCM =
        @"VOX_ADPCM";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_16 =
        @"NMS_ADPCM_16";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_24 =
        @"NMS_ADPCM_24";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_32 =
        @"NMS_ADPCM_32";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeG721_32 = @"G721_32";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeG723_24 = @"G723_24";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeG723_40 = @"G723_40";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_12 = @"DWVW_12";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_16 = @"DWVW_16";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_24 = @"DWVW_24";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_N = @"DWVW_N";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDPCM_8 = @"DPCM_8";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeDPCM_16 = @"DPCM_16";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeVorbis = @"Vorbis";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeOpus = @"Opus";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_16 = @"ALAC_16";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_20 = @"ALAC_20";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_24 = @"ALAC_24";
SFBAudioEncodingSettingsValueLibsndfileSubtype const SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_32 = @"ALAC_32";

SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianDefault =
        @"Default";
SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianLittle =
        @"Little";
SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianBig = @"Big";
SFBAudioEncodingSettingsValueLibsndfileFileEndian const SFBAudioEncodingSettingsValueLibsndfileFileEndianCPU = @"CPU";

/// Returns the major format corresponding to `pathExtension` or `0` if none
static int MajorFormatForExtension(NSString *pathExtension) {
    NSCParameterAssert(pathExtension != nil);

    int majorCount = 0;
    sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &majorCount, sizeof(int));

    for (int i = 0; i < majorCount; ++i) {
        SF_FORMAT_INFO formatInfo;
        formatInfo.format = i;
        if (!sf_command(NULL, SFC_GET_FORMAT_MAJOR, &formatInfo, sizeof(formatInfo))) {
            if ([pathExtension isEqualToString:[NSString stringWithUTF8String:formatInfo.extension]]) {
                return formatInfo.format;
            }
        } else {
            os_log_debug(gSFBAudioEncoderLog, "sf_command (SFC_GET_FORMAT_MAJOR) %d failed", i);
        }
    }

    return 0;
}

/// Returns the subtype corresponding to `format` or `0` if none
static int InferSubtypeFromFormat(AVAudioFormat *format) {
    NSCParameterAssert(format != nil);

    const AudioStreamBasicDescription *asbd = format.streamDescription;
    if (asbd->mFormatID != kAudioFormatLinearPCM) {
        return 0;
    }

    if ((asbd->mFormatFlags & kAudioFormatFlagIsFloat) == kAudioFormatFlagIsFloat) {
        if (asbd->mBitsPerChannel == 32) {
            return SF_FORMAT_FLOAT;
        }
        if (asbd->mBitsPerChannel == 64) {
            return SF_FORMAT_DOUBLE;
        }
    } else {
        BOOL isSignedInteger =
                (asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger) == kAudioFormatFlagIsSignedInteger;
        switch (asbd->mBitsPerChannel) {
        case 8:
            if (isSignedInteger) {
                return SF_FORMAT_PCM_S8;
            } else {
                return SF_FORMAT_PCM_U8;
            }
        case 16:
            if (isSignedInteger) {
                return SF_FORMAT_PCM_16;
            }
            break;
        case 24:
            if (isSignedInteger) {
                return SF_FORMAT_PCM_24;
            }
            break;
        case 32:
            if (isSignedInteger) {
                return SF_FORMAT_PCM_32;
            }
            break;
        }
    }

    return 0;
}

/// Fills a sndfile channel map with the corresponding channels from `channelDescriptions`
static void SndfileChannelMapWithChannelDescriptions(int *_Nonnull channel_map, int channels,
                                                     const AudioChannelDescription *channelDescriptions) {
    NSCParameterAssert(channel_map != NULL);
    NSCParameterAssert(channels > 0);
    NSCParameterAssert(channelDescriptions != NULL);

    for (int i = 0; i < channels; ++i) {
        switch (channelDescriptions[i].mChannelLabel) {
        case kAudioChannelLabel_Unused:
            channel_map[i] = SF_CHANNEL_MAP_INVALID;
            break;

        case kAudioChannelLabel_Mono:
            channel_map[i] = SF_CHANNEL_MAP_MONO;
            break;
        case kAudioChannelLabel_Left:
            channel_map[i] = SF_CHANNEL_MAP_LEFT;
            break;
        case kAudioChannelLabel_Right:
            channel_map[i] = SF_CHANNEL_MAP_RIGHT;
            break;
        case kAudioChannelLabel_Center:
            channel_map[i] = SF_CHANNEL_MAP_CENTER;
            break;

            // WAVEFORMATEXTENSIBLE standard channels (in dwChannelMask order)
            //            case kAudioChannelLabel_Left:
            //                channel_map[i] = SF_CHANNEL_MAP_FRONT_LEFT;
            //                break;
            //            case kAudioChannelLabel_Right:
            //                channel_map[i] = SF_CHANNEL_MAP_FRONT_RIGHT;
            //                break;
            //            case kAudioChannelLabel_Center:
            //                channel_map[i] = SF_CHANNEL_MAP_FRONT_CENTER;
            //                break;
        case kAudioChannelLabel_LFEScreen:
            channel_map[i] = SF_CHANNEL_MAP_LFE;
            break;
        case kAudioChannelLabel_LeftSurround:
            channel_map[i] = SF_CHANNEL_MAP_REAR_LEFT;
            break;
        case kAudioChannelLabel_RightSurround:
            channel_map[i] = SF_CHANNEL_MAP_REAR_RIGHT;
            break;
        case kAudioChannelLabel_LeftCenter:
            channel_map[i] = SF_CHANNEL_MAP_FRONT_LEFT_OF_CENTER;
            break;
        case kAudioChannelLabel_RightCenter:
            channel_map[i] = SF_CHANNEL_MAP_FRONT_RIGHT_OF_CENTER;
            break;
        case kAudioChannelLabel_CenterSurround:
            channel_map[i] = SF_CHANNEL_MAP_REAR_CENTER;
            break;
        case kAudioChannelLabel_LeftSurroundDirect:
            channel_map[i] = SF_CHANNEL_MAP_SIDE_LEFT;
            break;
        case kAudioChannelLabel_RightSurroundDirect:
            channel_map[i] = SF_CHANNEL_MAP_SIDE_RIGHT;
            break;
        case kAudioChannelLabel_TopCenterSurround:
            channel_map[i] = SF_CHANNEL_MAP_TOP_CENTER;
            break;
        case kAudioChannelLabel_VerticalHeightLeft:
            channel_map[i] = SF_CHANNEL_MAP_TOP_FRONT_LEFT;
            break;
        case kAudioChannelLabel_VerticalHeightCenter:
            channel_map[i] = SF_CHANNEL_MAP_TOP_FRONT_CENTER;
            break;
        case kAudioChannelLabel_VerticalHeightRight:
            channel_map[i] = SF_CHANNEL_MAP_TOP_FRONT_RIGHT;
            break;
        case kAudioChannelLabel_TopBackLeft:
            channel_map[i] = SF_CHANNEL_MAP_TOP_REAR_LEFT;
            break;
        case kAudioChannelLabel_TopBackCenter:
            channel_map[i] = SF_CHANNEL_MAP_TOP_REAR_CENTER;
            break;
        case kAudioChannelLabel_TopBackRight:
            channel_map[i] = SF_CHANNEL_MAP_TOP_REAR_RIGHT;
            break;

        case kAudioChannelLabel_Ambisonic_W:
            channel_map[i] = SF_CHANNEL_MAP_AMBISONIC_B_W;
            break;
        case kAudioChannelLabel_Ambisonic_X:
            channel_map[i] = SF_CHANNEL_MAP_AMBISONIC_B_X;
            break;
        case kAudioChannelLabel_Ambisonic_Y:
            channel_map[i] = SF_CHANNEL_MAP_AMBISONIC_B_Y;
            break;
        case kAudioChannelLabel_Ambisonic_Z:
            channel_map[i] = SF_CHANNEL_MAP_AMBISONIC_B_Z;
            break;

        default:
            os_log_error(gSFBAudioEncoderLog, "Unable to map channel label: %d", channelDescriptions[i].mChannelLabel);
            channel_map[i] = SF_CHANNEL_MAP_INVALID;
            break;
        }
    }
}

/// Fills a sndfile channel map with the corresponding channels from `channelBitmap`
static BOOL SndfileChannelMapWithChannelBitmap(int *_Nonnull channel_map, int channels,
                                               AudioChannelBitmap channelBitmap, NSError **error) {
    NSCParameterAssert(channel_map != NULL);
    NSCParameterAssert(channels > 0);

    UInt32 dataSize;
    OSStatus status = AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForBitmap, sizeof channelBitmap,
                                                 &channelBitmap, &dataSize);
    if (status != noErr) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        }
        return NO;
    }

    AudioChannelLayout *channelLayout = malloc(dataSize);
    if (!channelLayout) {
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    status = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForBitmap, sizeof(channelBitmap), &channelBitmap,
                                    &dataSize, channelLayout);
    if (status != noErr) {
        free(channelLayout);
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        }
        return NO;
    }

    SndfileChannelMapWithChannelDescriptions(channel_map, channels, channelLayout->mChannelDescriptions);

    free(channelLayout);

    return YES;
}

/// Fills a sndfile channel map with the corresponding channels from `layoutTag`
static BOOL SndfileChannelMapWithChannelLayoutTag(int *_Nonnull channel_map, int channels,
                                                  AudioChannelLayoutTag layoutTag, NSError **error) {
    NSCParameterAssert(channel_map != NULL);
    NSCParameterAssert(channels > 0);

    UInt32 dataSize;
    OSStatus status = AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag, sizeof layoutTag, &layoutTag,
                                                 &dataSize);
    if (status != noErr) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        }
        return NO;
    }

    AudioChannelLayout *channelLayout = malloc(dataSize);
    if (!channelLayout) {
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    status = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag, sizeof(layoutTag), &layoutTag, &dataSize,
                                    channelLayout);
    if (status != noErr) {
        free(channelLayout);
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        }
        return NO;
    }

    SndfileChannelMapWithChannelDescriptions(channel_map, channels, channelLayout->mChannelDescriptions);

    free(channelLayout);

    return YES;
}

/// Fills a sndfile channel map with the corresponding channels from `channelLayout`
static BOOL SndfileChannelMapFromChannelLayout(int *_Nonnull channel_map, int channels,
                                               AVAudioChannelLayout *_Nonnull channelLayout, NSError **error) {
    NSCParameterAssert(channel_map != NULL);
    NSCParameterAssert(channels > 0);
    NSCParameterAssert(channelLayout != nil);

    AudioChannelLayoutTag layoutTag = channelLayout.layoutTag;

    if (layoutTag == kAudioChannelLayoutTag_UseChannelDescriptions) {
        SndfileChannelMapWithChannelDescriptions(channel_map, channels, channelLayout.layout->mChannelDescriptions);
        return YES;
    }
    if (layoutTag == kAudioChannelLayoutTag_UseChannelBitmap) {
        return SndfileChannelMapWithChannelBitmap(channel_map, channels, channelLayout.layout->mChannelBitmap, error);
    }
    return SndfileChannelMapWithChannelLayoutTag(channel_map, channels, layoutTag, error);
}

enum WriteMethod {
    Unknown,
    Short,
    Int,
    Float,
    Double,
};

static sf_count_t my_sf_vio_get_filelen(void *user_data) {
    NSCParameterAssert(user_data != NULL);

    SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;
    NSInteger length;
    if (![encoder->_outputTarget getLength:&length error:nil]) {
        return -1;
    }
    return length;
}

static sf_count_t my_sf_vio_seek(sf_count_t offset, int whence, void *user_data) {
    NSCParameterAssert(user_data != NULL);

    SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;
    if (!encoder->_outputTarget.supportsSeeking) {
        return -1;
    }

    switch (whence) {
    case SEEK_SET:
        // offset remains unchanged
        break;
    case SEEK_CUR: {
        NSInteger outputTargetOffset;
        if ([encoder->_outputTarget getOffset:&outputTargetOffset error:nil]) {
            offset += outputTargetOffset;
        }
        break;
    }
    case SEEK_END: {
        NSInteger outputTargetLength;
        if ([encoder->_outputTarget getLength:&outputTargetLength error:nil]) {
            offset += outputTargetLength;
        }
        break;
    }
    }

    if (![encoder->_outputTarget seekToOffset:offset error:nil]) {
        return -1;
    }

    NSInteger outputTargetOffset;
    if (![encoder->_outputTarget getOffset:&outputTargetOffset error:nil]) {
        return -1;
    }

    return outputTargetOffset;
}

static sf_count_t my_sf_vio_read(void *ptr, sf_count_t count, void *user_data) {
    NSCParameterAssert(user_data != NULL);

    SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;

    NSInteger bytesRead;
    if (![encoder->_outputTarget readBytes:ptr length:count bytesRead:&bytesRead error:nil]) {
        return -1;
    }
    return bytesRead;
}

static sf_count_t my_sf_vio_write(const void *ptr, sf_count_t count, void *user_data) {
    NSCParameterAssert(user_data != NULL);

    SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;

    NSInteger bytesWritten;
    if (![encoder->_outputTarget writeBytes:ptr length:(NSInteger)count bytesWritten:&bytesWritten error:nil]) {
        return 0;
    }

    return bytesWritten;
}

static sf_count_t my_sf_vio_tell(void *user_data) {
    NSCParameterAssert(user_data != NULL);

    SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;
    NSInteger offset;
    if (![encoder->_outputTarget getOffset:&offset error:nil]) {
        return -1;
    }
    return offset;
}

@interface SFBLibsndfileEncoder () {
  @private
    SNDFILE *_sndfile;
    SF_INFO _sfinfo;
    enum WriteMethod _writeMethod;
}
@end

@implementation SFBLibsndfileEncoder

+ (void)load {
    [SFBAudioEncoder registerSubclass:[self class] priority:-50];
}

+ (NSSet *)supportedPathExtensions {
    static NSSet *pathExtensions = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        int majorCount = 0;
        sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &majorCount, sizeof(int));

        NSMutableSet *majorModeExtensions = [NSMutableSet setWithCapacity:(NSUInteger)majorCount];

        // Loop through each major mode
        for (int i = 0; i < majorCount; ++i) {
            SF_FORMAT_INFO formatInfo;
            formatInfo.format = i;
            if (!sf_command(NULL, SFC_GET_FORMAT_MAJOR, &formatInfo, sizeof(formatInfo))) {
                NSString *pathExtension = [NSString stringWithUTF8String:formatInfo.extension];
                if (pathExtension) {
                    [majorModeExtensions addObject:pathExtension];
                }
            } else {
                os_log_debug(gSFBAudioEncoderLog, "sf_command (SFC_GET_FORMAT_MAJOR) %d failed", i);
            }
        }

        pathExtensions = [majorModeExtensions copy];
    });

    return pathExtensions;
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet set];
}

+ (SFBAudioEncoderName)encoderName {
    return SFBAudioEncoderNameLibsndfile;
}

- (BOOL)encodingIsLossless {
    switch (_sfinfo.format & SF_FORMAT_TYPEMASK) {
    case SF_FORMAT_FLAC:
        return YES;
    default:
        break;
    }

    switch (_sfinfo.format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_S8:
    case SF_FORMAT_PCM_16:
    case SF_FORMAT_PCM_24:
    case SF_FORMAT_PCM_32:
        //    case SF_FORMAT_FLOAT:
        //    case SF_FORMAT_DOUBLE:
    case SF_FORMAT_ALAC_16:
    case SF_FORMAT_ALAC_20:
    case SF_FORMAT_ALAC_24:
    case SF_FORMAT_ALAC_32:
        return YES;
    default:
        // Be conservative and return NO for formats that aren't known to be lossless
        return NO;
    }
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat {
    NSParameterAssert(sourceFormat != nil);

    // Validate format
    const AudioStreamBasicDescription *asbd = sourceFormat.streamDescription;

    if (asbd->mFormatID != kAudioFormatLinearPCM) {
        return nil;
    }

    // Floating point
    if ((asbd->mFormatFlags & kAudioFormatFlagIsFloat) == kAudioFormatFlagIsFloat) {
        if (asbd->mBitsPerChannel == 32) {
            return [sourceFormat transformedToCommonFormat:AVAudioPCMFormatFloat32 interleaved:YES];
        }
        if (asbd->mBitsPerChannel == 64) {
            return [sourceFormat transformedToCommonFormat:AVAudioPCMFormatFloat64 interleaved:YES];
        }
    }
    // Integer
    else {
        AudioStreamBasicDescription streamDescription = {0};

        streamDescription.mFormatID = kAudioFormatLinearPCM;
        streamDescription.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

        streamDescription.mSampleRate = asbd->mSampleRate;
        streamDescription.mChannelsPerFrame = asbd->mChannelsPerFrame;

        streamDescription.mBitsPerChannel = ((asbd->mBitsPerChannel + 7) / 8) * 8;
        streamDescription.mFormatFlags |=
                streamDescription.mBitsPerChannel % 8 ? kAudioFormatFlagIsAlignedHigh : kAudioFormatFlagIsPacked;

        streamDescription.mBytesPerPacket =
                ((streamDescription.mBitsPerChannel + 7) / 8) * streamDescription.mChannelsPerFrame;
        streamDescription.mFramesPerPacket = 1;
        streamDescription.mBytesPerFrame = streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

        return [[AVAudioFormat alloc] initWithStreamDescription:&streamDescription
                                                  channelLayout:sourceFormat.channelLayout];
    }

    return nil;
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    int majorFormat = 0;
    SFBAudioEncodingSettingsValue majorFormatSetting =
            [_settings objectForKey:SFBAudioEncodingSettingsKeyLibsndfileMajorFormat];
    if (majorFormatSetting) {
        if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatWAV) {
            majorFormat = SF_FORMAT_WAV;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatAIFF) {
            majorFormat = SF_FORMAT_AIFF;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatAU) {
            majorFormat = SF_FORMAT_AU;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatRaw) {
            majorFormat = SF_FORMAT_RAW;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatPAF) {
            majorFormat = SF_FORMAT_PAF;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatSVX) {
            majorFormat = SF_FORMAT_SVX;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatNIST) {
            majorFormat = SF_FORMAT_NIST;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatVOC) {
            majorFormat = SF_FORMAT_VOC;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatIRCAM) {
            majorFormat = SF_FORMAT_IRCAM;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatW64) {
            majorFormat = SF_FORMAT_W64;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatMAT4) {
            majorFormat = SF_FORMAT_MAT4;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatMAT5) {
            majorFormat = SF_FORMAT_MAT5;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatPVF) {
            majorFormat = SF_FORMAT_PVF;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatXI) {
            majorFormat = SF_FORMAT_XI;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatHTK) {
            majorFormat = SF_FORMAT_HTK;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatSDS) {
            majorFormat = SF_FORMAT_SDS;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatAVR) {
            majorFormat = SF_FORMAT_AVR;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatWAVEX) {
            majorFormat = SF_FORMAT_WAVEX;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatSD2) {
            majorFormat = SF_FORMAT_SD2;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatFLAC) {
            majorFormat = SF_FORMAT_FLAC;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatCAF) {
            majorFormat = SF_FORMAT_CAF;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatWVE) {
            majorFormat = SF_FORMAT_WVE;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatOgg) {
            majorFormat = SF_FORMAT_OGG;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeOpus) {
            majorFormat = SF_FORMAT_OPUS;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatMPC2K) {
            majorFormat = SF_FORMAT_MPC2K;
        } else if (majorFormatSetting == SFBAudioEncodingSettingsValueLibsndfileMajorFormatRF64) {
            majorFormat = SF_FORMAT_RF64;
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring unknown libsndfile major format: %{public}@",
                         majorFormatSetting);
        }
    } else {
        majorFormat = MajorFormatForExtension(_outputTarget.url.pathExtension);
        os_log_info(gSFBAudioEncoderLog,
                    "SFBAudioEncodingSettingsKeyLibsndfileMajorFormat is not set: guessed 0x%x based on extension "
                    "\"%{public}@\"",
                    majorFormat, _outputTarget.url.pathExtension);
    }

    int subtype = 0;
    SFBAudioEncodingSettingsValue subtypeSetting =
            [_settings objectForKey:SFBAudioEncodingSettingsKeyLibsndfileSubtype];
    if (subtypeSetting) {
        if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_S8) {
            subtype = SF_FORMAT_PCM_S8;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_16) {
            subtype = SF_FORMAT_PCM_16;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_24) {
            subtype = SF_FORMAT_PCM_24;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_32) {
            subtype = SF_FORMAT_PCM_32;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypePCM_U8) {
            subtype = SF_FORMAT_PCM_U8;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeFloat) {
            subtype = SF_FORMAT_FLOAT;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeDouble) {
            subtype = SF_FORMAT_DOUBLE;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeµLAW) {
            subtype = SF_FORMAT_ULAW;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeALAW) {
            subtype = SF_FORMAT_ALAW;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeIMA_ADPCM) {
            subtype = SF_FORMAT_IMA_ADPCM;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeMS_ADPCM) {
            subtype = SF_FORMAT_MS_ADPCM;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeGSM610) {
            subtype = SF_FORMAT_GSM610;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeVOX_ADPCM) {
            subtype = SF_FORMAT_VOX_ADPCM;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_16) {
            subtype = SF_FORMAT_NMS_ADPCM_16;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_24) {
            subtype = SF_FORMAT_NMS_ADPCM_24;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeNMS_ADPCM_32) {
            subtype = SF_FORMAT_NMS_ADPCM_32;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeG721_32) {
            subtype = SF_FORMAT_G721_32;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeG723_24) {
            subtype = SF_FORMAT_G723_24;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeG723_40) {
            subtype = SF_FORMAT_G723_40;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_12) {
            subtype = SF_FORMAT_DWVW_12;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_16) {
            subtype = SF_FORMAT_DWVW_16;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_24) {
            subtype = SF_FORMAT_DWVW_24;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeDWVW_N) {
            subtype = SF_FORMAT_DWVW_N;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeDPCM_8) {
            subtype = SF_FORMAT_DPCM_8;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeDPCM_16) {
            subtype = SF_FORMAT_DPCM_16;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeVorbis) {
            subtype = SF_FORMAT_VORBIS;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeOpus) {
            subtype = SF_FORMAT_OPUS;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_16) {
            subtype = SF_FORMAT_ALAC_16;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_20) {
            subtype = SF_FORMAT_ALAC_20;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_24) {
            subtype = SF_FORMAT_ALAC_24;
        } else if (subtypeSetting == SFBAudioEncodingSettingsValueLibsndfileSubtypeALAC_32) {
            subtype = SF_FORMAT_ALAC_32;
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring unknown libsndfile subtype: %{public}@", subtypeSetting);
        }
    } else {
        subtype = InferSubtypeFromFormat(_processingFormat);
        os_log_info(gSFBAudioEncoderLog,
                    "SFBAudioEncodingSettingsKeyLibsndfileSubtype is not set: guessed 0x%x based on format %{public}@",
                    subtype, _processingFormat);
    }

    int endian = 0;
    NSNumber *fileEndianSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyLibsndfileFileEndian];
    if (fileEndianSetting != nil) {
        if (fileEndianSetting == SFBAudioEncodingSettingsValueLibsndfileFileEndianDefault) {
            endian = SF_ENDIAN_FILE;
        } else if (fileEndianSetting == SFBAudioEncodingSettingsValueLibsndfileFileEndianLittle) {
            endian = SF_ENDIAN_LITTLE;
        } else if (fileEndianSetting == SFBAudioEncodingSettingsValueLibsndfileFileEndianBig) {
            endian = SF_ENDIAN_BIG;
        } else if (fileEndianSetting == SFBAudioEncodingSettingsValueLibsndfileFileEndianCPU) {
            endian = SF_ENDIAN_CPU;
        } else {
            os_log_error(gSFBAudioEncoderLog, "Ignoring unknown libsndfile file endian-ness: %{public}@",
                         fileEndianSetting);
        }
    }

    _sfinfo.samplerate = (int)_processingFormat.sampleRate;
    _sfinfo.channels = (int)_processingFormat.channelCount;
    _sfinfo.format = majorFormat | subtype | endian;
    _sfinfo.seekable = 1;

    switch (subtype) {
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_S8:
    case SF_FORMAT_PCM_16:
        _writeMethod = Short;
        break;
    case SF_FORMAT_PCM_24:
    case SF_FORMAT_PCM_32:
        _writeMethod = Int;
        break;
    case SF_FORMAT_FLOAT:
        _writeMethod = Float;
        break;
    case SF_FORMAT_DOUBLE:
        _writeMethod = Double;
        break;
    default:
        os_log_error(gSFBAudioEncoderLog, "Unsupported subtype: 0x%x", subtype);
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    // Set up the virtual IO function pointers
    SF_VIRTUAL_IO virtualIO;
    virtualIO.get_filelen = my_sf_vio_get_filelen;
    virtualIO.seek = my_sf_vio_seek;
    virtualIO.read = my_sf_vio_read;
    virtualIO.write = my_sf_vio_write;
    virtualIO.tell = my_sf_vio_tell;

    // Open the output file
    _sndfile = sf_open_virtual(&virtualIO, SFM_WRITE, &_sfinfo, (__bridge void *)self);
    if (!_sndfile) {
        os_log_error(gSFBAudioEncoderLog, "sf_open_virtual failed: %{public}s", sf_error_number(sf_error(NULL)));
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInvalidFormat
                                     userInfo:@{
                                         NSLocalizedDescriptionKey : NSLocalizedString(
                                                 @"The requested output format is not supported by Libsndfile.", @""),
                                         NSLocalizedRecoverySuggestionErrorKey :
                                                 NSLocalizedString(@"The format is not supported.", @"")
                                     }];
        }
        return NO;
    }

    // Set up the channel map for sndfile
    AVAudioChannelLayout *processingFormatChannelLayout = _processingFormat.channelLayout;
    if (processingFormatChannelLayout) {
        int channel_map[_sfinfo.channels];
        // Don't propagate non-failing errors to the caller
        NSError *err = nil;
        if (SndfileChannelMapFromChannelLayout(channel_map, _sfinfo.channels, processingFormatChannelLayout, &err)) {
            // Not all formats supported by libsndfile handle channel maps; currently
            // only aiff, wav, rf64, and caf are supported (with opus marked as todo)
            // There is no way to distinguish lack of support from failure to set since
            // in both cases sf_command returns 0 and the underlying error is not set
            // Rather than failing just log the error and continue
            int result = sf_command(_sndfile, SFC_SET_CHANNEL_MAP_INFO, channel_map, (int)sizeof(channel_map));
            if (result != SF_TRUE) {
                os_log_error(gSFBAudioEncoderLog, "sf_command(SFC_SET_CHANNEL_MAP_INFO) failed: %{public}s",
                             sf_error_number(sf_error(_sndfile)));
            }
        } else {
            os_log_error(gSFBAudioEncoderLog, "Unable to determine libsndfile channel map for %{public}@: %{public}@",
                         processingFormatChannelLayout.layoutName, err);
        }
    }

    AudioStreamBasicDescription outputStreamDescription = {0};

    // Generic libsndfile format ID, will be set to something more specific if known
    outputStreamDescription.mFormatID = 'SNDF';

    outputStreamDescription.mSampleRate = _processingFormat.sampleRate;
    outputStreamDescription.mChannelsPerFrame = _processingFormat.channelCount;

    FillASBDWithSndfileFormat(&outputStreamDescription, _sfinfo.format);

    _outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription
                                                       channelLayout:processingFormatChannelLayout];

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_sndfile) {
        int result = sf_close(_sndfile);
        if (result) {
            os_log_error(gSFBAudioEncoderLog, "sf_close failed: %{public}s", sf_error_number(result));
        }
        _sndfile = NULL;
    }

    _writeMethod = Unknown;

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _sndfile != NULL;
}

- (AVAudioFramePosition)framePosition {
    return sf_seek(_sndfile, 0, SF_SEEK_CUR);
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    frameLength = MIN(frameLength, buffer.frameLength);
    if (frameLength == 0) {
        return YES;
    }

    sf_count_t framesWritten = 0;
    switch (_writeMethod) {
    case Short:
        framesWritten =
                sf_writef_short(_sndfile, (const short *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
        break;
    case Int:
        framesWritten = sf_writef_int(_sndfile, (const int *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
        break;
    case Float:
        framesWritten =
                sf_writef_float(_sndfile, (const float *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
        break;
    case Double:
        framesWritten =
                sf_writef_double(_sndfile, (const double *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
        break;
    default:
        os_log_error(gSFBAudioEncoderLog, "Unknown libsndfile write method: %d", _writeMethod);
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    if (framesWritten != frameLength) {
        os_log_info(gSFBAudioEncoderLog, "sf_writef_XXX wrote %lld/%u frames", framesWritten, frameLength);
    }

    int result = sf_error(_sndfile);
    if (result) {
        os_log_error(gSFBAudioEncoderLog, "sf_writef_XXX failed: %{public}s", sf_error_number(result));
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain
                                         code:SFBAudioEncoderErrorCodeInternalError
                                     userInfo:nil];
        }
        return NO;
    }

    return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error {
    return YES;
}

@end
