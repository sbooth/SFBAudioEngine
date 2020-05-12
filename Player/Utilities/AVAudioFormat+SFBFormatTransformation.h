/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVAudioFormat (SFBFormatTransformation)
- (nullable AVAudioFormat *)nonInterleavedEquivalent;
- (nullable AVAudioFormat *)interleavedEquivalent;
- (nullable AVAudioFormat *)standardEquivalent;
@end

NS_ASSUME_NONNULL_END
