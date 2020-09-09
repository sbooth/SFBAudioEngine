/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import <SFBAudioEngine/SFBInputSource.h>

NS_ASSUME_NONNULL_BEGIN

#pragma mark Audio Format Identifiers

/// Additional audio format IDs
typedef NS_ENUM(UInt32, SFBAudioFormatID) {
	/// Direct Stream Digital (DSD)
	SFBAudioFormatIDDirectStreamDigital 	= 'DSD ',
	/// DSD over PCM (DoP)
	SFBAudioFormatIDDoP 					= 'DoP ',
	/// Module
	SFBAudioFormatIDModule 					= 'MOD ',
	/// Monkey's Audio (APE)
	SFBAudioFormatIDMonkeysAudio 			= 'APE ',
	/// MPEG-1 (Layer I, II, or III)
	SFBAudioFormatIDMPEG1 					= 'MPG1',
	/// Musepack
	SFBAudioFormatIDMusepack 				= 'MPC ',
	/// Ogg Speex
	SFBAudioFormatIDSpeex 					= 'SPX ',
	/// True Audio
	SFBAudioFormatIDTrueAudio 				= 'TTA ',
	/// Ogg Vorbis
	SFBAudioFormatIDVorbis 					= 'OGG ',
	/// WavPack
	SFBAudioFormatIDWavPack 				= 'WV  '
} NS_SWIFT_NAME(AudioFormatID);

NS_SWIFT_NAME(AudioDecoding) @protocol SFBAudioDecoding

#pragma mark - Input

/// The \c SFBInputSource providing data to this decoder
@property (nonatomic, readonly) SFBInputSource *inputSource;

#pragma mark - Audio Format Information

/// The format of the native audio data
@property (nonatomic, readonly) AVAudioFormat *sourceFormat;

/// The format of audio of data provided by `-decodeIntoBuffer:error:`
@property (nonatomic, readonly) AVAudioFormat *processingFormat;

#pragma mark - Setup and Teardown

/// Opens the decoder for reading
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open());

/// Closes the decoder
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close());

/// Returns \c YES if the decoder is open
@property (nonatomic, readonly) BOOL isOpen;

#pragma mark - Decoding

/// Decodes audio
/// @param buffer A buffer to receive the decoded audio
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error NS_SWIFT_NAME(decode(into:));

#pragma mark - Seeking

/// Returns \c YES if the decoder is seekable
@property (nonatomic, readonly) BOOL supportsSeeking;

@end

NS_ASSUME_NONNULL_END
