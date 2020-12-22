/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio boolean control
NS_SWIFT_NAME(BooleanControl) @interface SFBBooleanControl : SFBAudioControl

/// Returns the control's value or \c nil on error
/// @note This corresponds to \c kAudioBooleanControlPropertyValue
@property (nonatomic, nullable, readonly) NSNumber *value NS_REFINED_FOR_SWIFT;
/// Sets the control's value
/// @note This corresponds to \c kAudioBooleanControlPropertyValue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setValue:(BOOL)value error:(NSError **)error NS_SWIFT_NAME(setValue(_:));

@end

NS_ASSUME_NONNULL_END
