/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio control
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(AudioControl) @interface SFBAudioControl : SFBAudioObject

/// Returns the scope or \c nil on error
/// @note This corresponds to \c kAudioControlPropertyScope
@property (nonatomic, readonly) NSNumber *scope;

/// Returns the element or \c nil on error
/// @note This corresponds to \c kAudioControlPropertyElement
@property (nonatomic, readonly) NSNumber *element;

@end

NS_ASSUME_NONNULL_END
