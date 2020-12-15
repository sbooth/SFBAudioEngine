/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio slider control
NS_SWIFT_NAME(SliderControl) @interface SFBSliderControl : SFBAudioControl

/// Returns the control's value or \c 0 on error
/// @note This corresponds to \c kAudioSliderControlPropertyValue
@property (nonatomic, readonly) UInt32 value;
/// Returns an array of available values or \c nil on error
/// @note This corresponds to \c kAudioSliderControlPropertyRange
@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *range;

@end

NS_ASSUME_NONNULL_END
