/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Useful functions for PCM buffer manipulation
@interface AVAudioPCMBuffer (SFBBufferUtilities)
/// Appends the contents of \c buffer to \c self
/// @note The format of \c buffer must match the format of \c self
/// @return The number of frames appended
- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer NS_SWIFT_NAME(append(_:));
/// Appends frames from \c buffer starting at \c readOffset to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param readOffset The desired starting offset in \c buffer
/// @return The number of frames appended
- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset NS_SWIFT_NAME(append(_:from:));
/// Appends at most \c frameLength frames from \c buffer starting at \c readOffset to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param readOffset The desired starting offset in \c buffer
/// @param frameLength The desired number of frames
/// @return The number of frames appended
- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(append(_:from:length:));

/// Copies at most \c frameLength frames from \c buffer starting at \c readOffset to \c self
/// @note The format of \c buffer must match the format of \c self
/// @param readOffset The desired starting offset in \c buffer
/// @param frameLength The desired number of frames
/// @return The number of frames copied
- (AVAudioFrameCount)copyFromBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(copy(_:from:length:));
/// Copies at most \c frameLength frames from \c buffer starting at \c readOffset to \c self starting at \c writeOffset
/// @note The format of \c buffer must match the format of \c self
/// @param readOffset The desired starting offset in \c buffer
/// @param frameLength The desired number of frames
/// @param writeOffset The desired starting offset in \c self
/// @return The number of frames copied
- (AVAudioFrameCount)copyFromBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength writeOffset:(AVAudioFrameCount)writeOffset NS_SWIFT_NAME(copy(_:from:length:at:));

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
