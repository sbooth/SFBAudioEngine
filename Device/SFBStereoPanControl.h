/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio stereo pan control
NS_SWIFT_NAME(StereoPanControl) @interface SFBStereoPanControl : SFBAudioControl

/// Returns the control's value or \c nil on error
/// @note This corresponds to \c kAudioStereoPanControlPropertyValue
@property (nonatomic, nullable, readonly) NSNumber *value NS_REFINED_FOR_SWIFT;

/// Returns the control's panning channels or \c nil on error
/// @note This corresponds to \c kAudioStereoPanControlPropertyPanningChannels
@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *panningChannels NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
