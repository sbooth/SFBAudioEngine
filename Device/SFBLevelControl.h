/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio level control
NS_SWIFT_NAME(LevelControl) @interface SFBLevelControl : SFBAudioControl

/// Returns the control's scalar value
/// @note This corresponds to \c kAudioLevelControlPropertyScalarValue
@property (nonatomic, readonly) Float32 scalarValue;

/// Returns the control's decibel value
/// @note This corresponds to \c kAudioLevelControlPropertyDecibelValue
@property (nonatomic, readonly) Float32 decibelValue;

/// Returns the control's decibel range or \c nil on error
/// @note This corresponds to \c kAudioLevelControlPropertyDecibelRange
@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *decibelRange;

/// Converts and returns \c scalar converted to decibels
/// @note This corresponds to \c kAudioLevelControlPropertyConvertScalarToDecibels
- (Float32)convertToDecibelsFromScalar:(Float32)scalar;

/// Converts and returns \c decibels converted to scalar
/// @note This corresponds to \c kAudioLevelControlPropertyConvertDecibelsToScalar
- (Float32)convertToScalarFromDecibels:(Float32)decibels;

@end

NS_ASSUME_NONNULL_END
