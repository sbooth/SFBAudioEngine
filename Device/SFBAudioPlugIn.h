/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

@class SFBAudioDevice, SFBAudioBox, SFBClockDevice;

/// An audio plug in
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(AudioPlugIn) @interface SFBAudioPlugIn : SFBAudioObject

/// Returns an array of available audio plug ins or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyPlugInList on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioPlugIn *> *plugIns;

/// Returns the bundle ID or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyBundleID
@property (nonatomic, nullable, readonly) NSString *bundleID;
/// Returns an array  of audio devices provided by the plug in or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyDeviceList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *devices;
/// Returns an array  of audio boxes provided by the plug in or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyBoxList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioBox *> *boxes;
/// Returns an array  of audio clock devices provided by the plug in or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyClockDeviceList
@property (nonatomic, nullable, readonly) NSArray<SFBClockDevice *> *clockDevices;

@end

NS_ASSUME_NONNULL_END
