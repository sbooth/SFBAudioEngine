/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVAudioPCMBuffer (SFBBufferUtilities)
- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer NS_SWIFT_NAME(append(_:));
- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength  NS_SWIFT_NAME(append(_:from:length:));

- (AVAudioFrameCount)copyFromBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(copy(_:from:length:));
- (AVAudioFrameCount)copyFromBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength writeOffset:(AVAudioFrameCount)writeOffset NS_SWIFT_NAME(copy(_:from:length:at:));

- (AVAudioFrameCount)trimAtOffset:(AVAudioFrameCount)offset frameLength:(AVAudioFrameCount)frameLength NS_SWIFT_NAME(trim(at:length:));
@end

NS_ASSUME_NONNULL_END
