/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVAudioFormat (SFBFormatTransformation)
/// Returns a copy of \c self converted to the equivalent non-interleaved format
/// @note Returns \c nil for non-PCM formats
- (nullable AVAudioFormat *)nonInterleavedEquivalent;
/// Returns a copy of \c self converted to the equivalent interleaved format
/// @note Returns \c nil for non-PCM formats
- (nullable AVAudioFormat *)interleavedEquivalent;
/// Returns a copy of \c self converted to the equivalent standard format
- (nullable AVAudioFormat *)standardEquivalent;
@end

NS_ASSUME_NONNULL_END
