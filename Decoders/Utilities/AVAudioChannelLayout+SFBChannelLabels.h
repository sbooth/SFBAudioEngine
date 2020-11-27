/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Functions for building channel layouts from channel labels
@interface AVAudioChannelLayout (SFBChannelLabels)
/// Returns an initialized \c AVAudioChannelLayout object with the specified channel labels or \c nil on failure
/// @param count The number of channel labels
+ (nullable instancetype)layoutWithChannelLabels:(AVAudioChannelCount)count, ...;
/// Returns an initialized \c AVAudioChannelLayout object with the specified channel labels or \c nil on failure
/// @param channelLabels An array of channel labels
/// @param count The number of channel labels
+ (nullable instancetype)layoutWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count;
/// Returns an initialized \c AVAudioChannelLayout object with the specified channel labels or \c nil on failure
/// @param count The number of channel labels
- (nullable instancetype)initWithChannelLabels:(AVAudioChannelCount)count, ...;
/// Returns an initialized \c AVAudioChannelLayout object with the specified channel labels or \c nil on failure
/// @param count The number of channel labels
/// @param ap A variadic argument list containing \c count \c AudioChannelLabel parameters
- (nullable instancetype)initWithChannelLabels:(AVAudioChannelCount)count ap:(va_list)ap;
/// Returns an initialized \c AVAudioChannelLayout object with the specified channel labels or \c nil on failure
/// @param channelLabels An array of channel labels
/// @param count The number of channel labels
- (nullable instancetype)initWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count;
@end

NS_ASSUME_NONNULL_END
