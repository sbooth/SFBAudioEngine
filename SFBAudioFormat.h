/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <CoreAudio/CoreAudioTypes.h>
#import <Foundation/Foundation.h>

#import "SFBAudioChannelLayout.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief Additional audio format IDs */
typedef NS_ENUM(UInt32, SFBAudioFormatID) {
	SFBAudioFormatIDDirectStreamDigital 	= 'DSD ',	/*!< Direct Stream Digital (DSD) */
	SFBAudioFormatIDDoP 					= 'DoP ',	/*!< DSD over PCM (DoP) */
	SFBAudioFormatIDFLAC 					= 'FLAC',	/*!< Free Lossless Audio Codec (FLAC) */
	SFBAudioFormatIDModule 					= 'MOD ',	/*!< Module */
	SFBAudioFormatIDMonkeysAudio 			= 'APE ',	/*!< Monkey's Audio (APE) */
	SFBAudioFormatIDMPEG1 					= 'MPG1',	/*!< MPEG-1 (Layer I, II, or III) */
	SFBAudioFormatIDMusepack 				= 'MPC ',	/*!< Musepack */
	SFBAudioFormatIDOpus 					= 'OPUS',	/*!< Ogg Opus */
	SFBAudioFormatIDSpeex 					= 'SPX ',	/*!< Ogg Speex */
	SFBAudioFormatIDTrueAudio 				= 'TTA ',	/*!< True Audio */
	SFBAudioFormatIDVorbis 					= 'OGG ',	/*!< Ogg Vorbis */
	SFBAudioFormatIDWavPack 				= 'WV  '	/*!< WavPack */
};

/*! @brief Common audio formats */
typedef NS_ENUM(NSUInteger, SFBAudioFormatCommonPCMFormat) {
	SFBAudioFormatCommonPCMFormatFloat32 	= 1, /*!< Native-endian \c float, the standard format */
	SFBAudioFormatCommonPCMFormatFloat64 	= 2, /*!< Native-endian \c double */
	SFBAudioFormatCommonPCMFormatInt16 		= 3, /*!< Native-endian signed 16-bit integers */
	SFBAudioFormatCommonPCMFormatInt32 		= 4, /*!< Native-endian signed 32-bit integers */
};

/*! @brief Immutable thin wrapper around \c AudioStreamBasicDescription with support for DSD */
@interface SFBAudioFormat : NSObject <NSCopying>

- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithCommonPCMFormat:(SFBAudioFormatCommonPCMFormat)format sampleRate:(double)sampleRate channels:(NSInteger)channels interleaved:(BOOL)interleaved;

/*! @brief Returns an initialized  \c SFBAudioFormat object for the specified format*/
- (instancetype)initWithStreamDescription:(AudioStreamBasicDescription)streamDescription;

/*! @brief Returns an initialized  \c SFBAudioFormat object for the specified format and channel layout */
- (instancetype)initWithStreamDescription:(AudioStreamBasicDescription)streamDescription channelLayout:(nullable SFBAudioChannelLayout *)channelLayout NS_DESIGNATED_INITIALIZER;

/*! @brief Returns \c YES if this format represents interleaved data */
@property (nonatomic, readonly) BOOL isInterleaved;

/*! @brief Returns \c YES if this format represents PCM audio data */
@property (nonatomic, readonly) BOOL isPCM;

/*! @brief Returns \c YES if this format represents DSD audio data */
@property (nonatomic, readonly) BOOL isDSD;

/*! @brief Returns \c YES if this format represents DoP audio data */
@property (nonatomic, readonly) BOOL isDoP;

/*! @brief Returns \c YES if this format represents big-endian ordered data */
@property (nonatomic, readonly) BOOL isBigEndian;

/*! @brief Returns \c YES if this format represents native-endian ordered data */
@property (nonatomic, readonly) BOOL isNativeEndian;

/*! @brief Returns the number of channels of audio data */
@property (nonatomic, readonly) NSInteger channelCount;

/*! @brief Returns the sample rate of this format in Hz */
@property (nonatomic, readonly) double sampleRate;

/*! @brief Returns a \c const pointer to this object's internal \c AudioStreamBasicDescription */
@property (nonatomic, readonly) const AudioStreamBasicDescription *streamDescription NS_RETURNS_INNER_POINTER;

/*! @brief Returns the channel layout for this format */
@property (nonatomic, nullable, readonly) SFBAudioChannelLayout *channelLayout;

/*!
 * @brief Returns the number of bytes required for the specified number of frames
 * @param frameCount The desired number of frames
 * @return The number of bytes required for \c frameCount frames
 */
- (NSInteger)frameCountToByteCount:(NSInteger)frameCount;

/*!
 * @brief Returns the number of frames contained for the specified number of bytes
 * @param byteCount The desired number of bytes
 * @return The number of frames   for \c byteCount bytes
 */
- (NSInteger)byteCountToFrameCount:(NSInteger)byteCount;

@end

NS_ASSUME_NONNULL_END
