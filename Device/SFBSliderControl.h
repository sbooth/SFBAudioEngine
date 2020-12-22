/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio slider control
NS_SWIFT_NAME(SliderControl) @interface SFBSliderControl : SFBAudioControl

/// Returns the control's value or \c nil on error
/// @note This corresponds to \c kAudioSliderControlPropertyValue
@property (nonatomic, nullable, readonly) NSNumber *value NS_REFINED_FOR_SWIFT;
/// Sets the control's value
/// @note This corresponds to \c kAudioSliderControlPropertyValue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setValue:(unsigned int)value error:(NSError **)error NS_SWIFT_NAME(setValue(_:));

/// Returns an array of available values or \c nil on error
/// @note This corresponds to \c kAudioSliderControlPropertyRange
@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *range NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
