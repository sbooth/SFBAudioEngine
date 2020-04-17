/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <AVFoundation/AVFoundation.h>

@interface AVAudioChannelLayout (SFBChannelLabels)
+ (instancetype)layoutWithChannelLabels:(AVAudioChannelCount)numberChannelLabels, ...;
- (instancetype)initWithChannelLabels:(AVAudioChannelCount)numberChannelLabels, ...;
- (instancetype)initWithChannelLabels:(AVAudioChannelCount)numberChannelLabels ap:(va_list)ap;
@end
