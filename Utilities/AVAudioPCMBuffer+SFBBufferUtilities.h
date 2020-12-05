/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Useful functions for PCM buffer manipulation
@interface AVAudioPCMBuffer (SFBBufferUtilities)
/// Prepends the contents of \c buffer to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @return The number of frames prepended
- (AVAudioFrameCount)prependContentsOfBuffer:(AVAudioPCMBuffer *)buffer NS_SWIFT_NAME(prepend(_:));
/// Prepends frames from \c buffer starting at \c offset to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @param offset The desired starting offset in \c buffer
/// @return The number of frames prepended
- (AVAudioFrameCount)prependFromBuffer:(AVAudioPCMBuffer *)buffer readingFromOffset:(AVAudioFrameCount)offset NS_SWIFT_NAME(prepend(_:from:));
/// Prepends at most \c frameLength frames from \c buffer starting at \c offset to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @param offset The desired starting offset in \c buffer
/// @param frameLength The desired number of frames
/// @return The number of frames prepended
- (AVAudioFrameCount)prependFromBuffer:(AVAudioPCMBuffer *)buffer readingFromOffset:(AVAudioFrameCount)offset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(prepend(_:from:length:));

/// Appends the contents of \c buffer to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @return The number of frames appended
- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer NS_SWIFT_NAME(append(_:));
/// Appends frames from \c buffer starting at \c offset to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @param offset The desired starting offset in \c buffer
/// @return The number of frames appended
- (AVAudioFrameCount)appendFromBuffer:(AVAudioPCMBuffer *)buffer readingFromOffset:(AVAudioFrameCount)offset NS_SWIFT_NAME(append(_:from:));
/// Appends at most \c frameLength frames from \c buffer starting at \c offset to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @param offset The desired starting offset in \c buffer
/// @param frameLength The desired number of frames
/// @return The number of frames appended
- (AVAudioFrameCount)appendFromBuffer:(AVAudioPCMBuffer *)buffer readingFromOffset:(AVAudioFrameCount)offset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(append(_:from:length:));

/// Inserts the contents of \c buffer in \c self starting at \c offset
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @param offset The desired starting offset in \c self
/// @return The number of frames inserted
- (AVAudioFrameCount)insertContentsOfBuffer:(AVAudioPCMBuffer *)buffer atOffset:(AVAudioFrameCount)offset NS_SWIFT_NAME(insert(_:at:));

/// Inserts at most \c readLength frames from \c buffer starting at \c readOffset to \c self starting at \c writeOffset
/// @note The format of \c buffer must match the format of \c self
/// @param buffer A buffer of audio data
/// @param readOffset The desired starting offset in \c buffer
/// @param frameLength The desired number of frames
/// @param writeOffset The desired starting offset in \c self
/// @return The number of frames inserted
- (AVAudioFrameCount)insertFromBuffer:(AVAudioPCMBuffer *)buffer readingFromOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength atOffset:(AVAudioFrameCount)writeOffset NS_SWIFT_NAME(insert(_:from:length:at:));

/// Deletes at most the first \c frameLength frames from \c self
/// @param frameLength The desired number of frames
/// @return The number of frames deleted
- (AVAudioFrameCount)trimFirst:(AVAudioFrameCount)frameLength;

/// Deletes at most the last \c frameLength frames from \c self
/// @param frameLength The desired number of frames
/// @return The number of frames deleted
- (AVAudioFrameCount)trimLast:(AVAudioFrameCount)frameLength;

/// Deletes at most \c frameLength frames from \c self starting at \c offset
/// @param offset The desired starting offset
/// @param frameLength The desired number of frames
/// @return The number of frames deleted
- (AVAudioFrameCount)trimAtOffset:(AVAudioFrameCount)offset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(trim(at:length:));

/// Fills the remainder of \c self with silence
/// @return The number of frames of silence appended
- (AVAudioFrameCount)fillRemainderWithSilence;
/// Appends at most \c frameLength frames of silence to \c self
/// @param frameLength The desired number of frames
/// @return The number of frames of silence appended
- (AVAudioFrameCount)appendSilenceOfLength:(AVAudioFrameCount)frameLength;
/// Inserts at most \c frameLength frames of silence to \c self starting at \c offset
/// @param offset The desired starting offset
/// @param frameLength The desired number of frames
/// @return The number of frames of silence inserted
- (AVAudioFrameCount)insertSilenceAtOffset:(AVAudioFrameCount)offset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(silence(at:length:));

/// Returns \c YES if \c self.frameLength == \c 0
- (BOOL)isEmpty;
/// Returns \c YES if \c self.frameLength == \c self.frameCapacity
- (BOOL)isFull;

/// Returns \c YES if \c self contains only digital silence
- (BOOL)isDigitalSilence;
@end

NS_ASSUME_NONNULL_END
