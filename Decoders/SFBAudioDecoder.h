/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "SFBInputSource.h"

NS_ASSUME_NONNULL_BEGIN

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

/*! @brief The \c NSErrorDomain used by \c SFBAudioDecoder and subclasses */
extern NSErrorDomain const SFBAudioDecoderErrorDomain;

/*! @brief Possible \c NSError  error codes used by \c SFBAudioDecoder */
typedef NS_ERROR_ENUM(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCode) {
	SFBAudioDecoderErrorCodeFileNotFound	= 0,		/*!< File not found */
	SFBAudioDecoderErrorCodeInputOutput		= 1			/*!< Input/output error */
};

/*! @brief A decoder providing audio as PCM */
@interface SFBAudioDecoder : NSObject

/*! @brief Returns a set containing the supported path extensions */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/*!@brief Returns  a set containing the supported MIME types */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/*! @brief Tests whether a file extension is supported */
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/*! @brief Tests whether a MIME type is supported */
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url;
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) SFBInputSource *inputSource;

@property (nonatomic, readonly) AVAudioFormat *sourceFormat;
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

/*!
 * @brief Opens the decoder for reading
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open()) NS_REQUIRES_SUPER;

/*!
 * Closes the decoder
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close()) NS_REQUIRES_SUPER;

/*! @brief Returns \c YES if the decoder is open */
@property (nonatomic, readonly) BOOL isOpen;

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(decode(into:));
- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error NS_SWIFT_NAME(decode(into:length:));

@property (nonatomic, readonly) AVAudioFramePosition currentFrame;
@property (nonatomic, readonly) AVAudioFramePosition totalFrames;
@property (nonatomic, readonly) AVAudioFramePosition framesRemaining;

@property (nonatomic, readonly) BOOL supportsSeeking;

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error;

@property (nonatomic, nullable) id representedObject;

@end

@interface SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)
+ (void)registerSubclass:(Class)subclass;
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
