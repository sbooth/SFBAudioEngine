/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio level control
NS_SWIFT_NAME(LevelControl) @interface SFBLevelControl : SFBAudioControl

/// Returns the control's scalar value or \c nil on error
/// @note This corresponds to \c kAudioLevelControlPropertyScalarValue
@property (nonatomic, nullable, readonly) NSNumber *scalarValue NS_REFINED_FOR_SWIFT;

/// Returns the control's decibel value or \c nil on error
/// @note This corresponds to \c kAudioLevelControlPropertyDecibelValue
@property (nonatomic, nullable, readonly) NSNumber *decibelValue NS_REFINED_FOR_SWIFT;

/// Returns the control's decibel range or \c nil on error
/// @note This corresponds to \c kAudioLevelControlPropertyDecibelRange
/// @note The return value contains a wrapped \c AudioValueRange  structure
@property (nonatomic, nullable, readonly) NSValue *decibelRange NS_REFINED_FOR_SWIFT;

/// Converts \c scalar to decibels and returns the converted value or \c nil on error
/// @note This corresponds to \c kAudioLevelControlPropertyConvertScalarToDecibels
- (nullable NSNumber *)convertToDecibelsFromScalar:(float)scalar error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Converts \c decibels to scalar and returns the converted value or \c nil on error
/// @note This corresponds to \c kAudioLevelControlPropertyConvertDecibelsToScalar
- (nullable NSNumber *)convertToScalarFromDecibels:(float)decibels error:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
