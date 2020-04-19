/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDecoding.h"

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(PCMDecoding) @protocol SFBPCMDecoding <SFBAudioDecoding>

#pragma mark - Position and Length Information

/*! @brief Returns the decoder's current frame position or \c -1 if unknown */
@property (nonatomic, readonly) AVAudioFramePosition framePosition;

/*! @brief Returns the decoder's length in frames or \c -1 if unknown */
@property (nonatomic, readonly) AVAudioFramePosition frameLength;

#pragma mark - Decoding

/*!
 * @brief Decodes audio
 * @param buffer A buffer to receive the decoded audio
 * @param frameLength The desired number of audio frames
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error NS_SWIFT_NAME(decode(into:length:));

#pragma mark - Seeking

/*!
 * @brief Seeks to the specified frame
 * @param frame The desired frame
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
