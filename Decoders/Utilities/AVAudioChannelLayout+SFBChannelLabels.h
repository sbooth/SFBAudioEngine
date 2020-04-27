/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */


#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVAudioChannelLayout (SFBChannelLabels)
+ (nullable instancetype)layoutWithChannelLabels:(AVAudioChannelCount)count, ...;
+ (nullable instancetype)layoutWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count;
- (nullable instancetype)initWithChannelLabels:(AVAudioChannelCount)count, ...;
- (nullable instancetype)initWithChannelLabels:(AVAudioChannelCount)count ap:(va_list)ap;
- (nullable instancetype)initWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count;
@end

NS_ASSUME_NONNULL_END
