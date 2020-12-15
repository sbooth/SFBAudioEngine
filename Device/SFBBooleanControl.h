/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio boolean control
NS_SWIFT_NAME(BooleanControl) @interface SFBBooleanControl : SFBAudioControl

/// Returns the control's value
/// @note This corresponds to \c kAudioBooleanControlPropertyValue
@property (nonatomic, readonly) BOOL value;

@end

NS_ASSUME_NONNULL_END
