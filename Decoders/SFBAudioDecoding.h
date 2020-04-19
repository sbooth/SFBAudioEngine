/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import "SFBInputSource.h"

NS_ASSUME_NONNULL_BEGIN

#pragma mark Audio Format Identifiers

/*! @brief Additional audio format IDs */
typedef NS_ENUM(UInt32, SFBAudioFormatID) {
	SFBAudioFormatIDDirectStreamDigital 	= 'DSD ',	/*!< Direct Stream Digital (DSD) */
	SFBAudioFormatIDDoP 					= 'DoP ',	/*!< DSD over PCM (DoP) */
	SFBAudioFormatIDModule 					= 'MOD ',	/*!< Module */
	SFBAudioFormatIDMonkeysAudio 			= 'APE ',	/*!< Monkey's Audio (APE) */
	SFBAudioFormatIDMPEG1 					= 'MPG1',	/*!< MPEG-1 (Layer I, II, or III) */
	SFBAudioFormatIDMusepack 				= 'MPC ',	/*!< Musepack */
	SFBAudioFormatIDSpeex 					= 'SPX ',	/*!< Ogg Speex */
	SFBAudioFormatIDTrueAudio 				= 'TTA ',	/*!< True Audio */
	SFBAudioFormatIDVorbis 					= 'OGG ',	/*!< Ogg Vorbis */
	SFBAudioFormatIDWavPack 				= 'WV  '	/*!< WavPack */
};

NS_SWIFT_NAME(AudioDecoding) @protocol SFBAudioDecoding

#pragma mark - Input

/*! @brief The \c SFBInputSource providing data to this decoder */
@property (nonatomic, readonly) SFBInputSource *inputSource;

#pragma mark - Audio Format Information

/*! @brief The format of the native audio data */
@property (nonatomic, readonly) AVAudioFormat *sourceFormat;

/*! @brief The format of audio of data provided by `-decodeIntoBuffer:error:` */
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

#pragma mark - Setup and Teardown

/*!
 * @brief Opens the decoder for reading
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/*!
 * Closes the decoder
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/*! @brief Returns \c YES if the decoder is open */
@property (nonatomic, readonly) BOOL isOpen;

#pragma mark - Decoding

/*!
 * @brief Decodes audio
 * @param buffer A buffer to receive the decoded audio
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(decode(into:));

#pragma mark - Seeking

/*! @brief Returns \c YES if the decoder is seekable */
@property (nonatomic, readonly) BOOL supportsSeeking;

@end

NS_ASSUME_NONNULL_END
