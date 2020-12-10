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
/// Returns an initialized \c AVAudioChannelLayout object according to the specified channel label string or \c nil on failure
/// @param channelLabelString A string containing the channel labels
+ (nullable instancetype)layoutWithChannelLabelString:(NSString *)channelLabelString;
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
/// Returns an initialized \c AVAudioChannelLayout object according to the specified channel label string or \c nil on failure
/// @note The string comparisons are case-insensitive
///
/// The following channel label strings are recognized:
/// L \c kAudioChannelLabel_Left
/// R \c kAudioChannelLabel_Right
/// C \c kAudioChannelLabel_Center
/// LFE \c kAudioChannelLabel_LFEScreen
/// Ls \c kAudioChannelLabel_LeftSurround
/// Rs \c kAudioChannelLabel_RightSurround
/// Lc \c kAudioChannelLabel_LeftCenter
/// Rc \c kAudioChannelLabel_RightCenter
/// Cs \c kAudioChannelLabel_CenterSurround
/// Lsd \c kAudioChannelLabel_LeftSurroundDirect
/// Rsd \c kAudioChannelLabel_RightSurroundDirect
/// Tcs \c kAudioChannelLabel_TopCenterSurround
/// Vhl \c kAudioChannelLabel_VerticalHeightLeft
/// Vhc \c kAudioChannelLabel_VerticalHeightCenter
/// Vhl \c kAudioChannelLabel_VerticalHeightRight
/// RLs \c kAudioChannelLabel_RearSurroundLeft
/// RRs \c kAudioChannelLabel_RearSurroundRight
/// Lw \c kAudioChannelLabel_LeftWide
/// Rw \c kAudioChannelLabel_RightWide
/// All other strings are mapped to \c kAudioChannelLabel_Unknown
/// @param channelLabelString A string containing the channel labels
- (nullable instancetype)initWithChannelLabelString:(NSString *)channelLabelString;
@end

NS_ASSUME_NONNULL_END
