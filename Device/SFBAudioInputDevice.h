/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

NS_ASSUME_NONNULL_BEGIN

/// An \c SFBAudioDevice supporting input
NS_SWIFT_NAME(AudioInputDevice) @interface SFBAudioInputDevice: SFBAudioDevice

/// Returns an array of available audio devices supporting input or \c nil on error
/// @note A device supports input if it has a buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMaster }
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioInputDevice *> *inputDevices;

/// Returns the default input device or \c nil on error
@property (class, nonatomic, nullable, readonly) SFBAudioInputDevice *defaultInputDevice;

@end

NS_ASSUME_NONNULL_END
