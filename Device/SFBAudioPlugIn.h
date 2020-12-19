/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

@class SFBAudioDevice, SFBAudioBox, SFBClockDevice;

NS_ASSUME_NONNULL_BEGIN

/// An audio plug in
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(AudioPlugIn) @interface SFBAudioPlugIn : SFBAudioObject

/// Returns an array of available audio plug ins or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyPlugInList on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioPlugIn *> *plugIns;

/// Returns an initialized \c SFBAudioPlugIn object with the specified bundle ID
/// @param bundleID The desired bundle ID
/// @return An initialized \c SFBAudioPlugIn object or \c nil if \c bundleID is invalid or unknown
- (nullable instancetype)initWithBundleID:(NSString *)bundleID;

/// Returns the bundle ID or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyBundleID
@property (nonatomic, nullable, readonly) NSString *bundleID NS_REFINED_FOR_SWIFT;
/// Returns an array  of audio devices provided by the plug in or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyDeviceList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *devices NS_REFINED_FOR_SWIFT;
/// Returns the audio device provided by the plug in with the specified UID or \c nil if unknown
/// @note This corresponds to \c kAudioPlugInPropertyTranslateUIDToDevice
- (nullable SFBAudioDevice *)deviceForUID:(NSString *)deviceUID NS_SWIFT_NAME(device(_:));
/// Returns an array  of audio boxes provided by the plug in or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyBoxList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioBox *> *boxes NS_REFINED_FOR_SWIFT;
/// Returns the audio box provided by the plug in with the specified UID or \c nil if unknown
/// @note This corresponds to \c kAudioPlugInPropertyTranslateUIDToBox
- (nullable SFBAudioBox *)boxForUID:(NSString *)boxUID NS_SWIFT_NAME(box(_:));
/// Returns an array  of audio clock devices provided by the plug in or \c nil on error
/// @note This corresponds to \c kAudioPlugInPropertyClockDeviceList
@property (nonatomic, nullable, readonly) NSArray<SFBClockDevice *> *clockDevices NS_REFINED_FOR_SWIFT;
/// Returns the clock device provided by the plug in with the specified UID or \c nil if unknown
/// @note This corresponds to \c kAudioPlugInPropertyTranslateUIDToClockDevice
- (nullable SFBClockDevice *)clockDeviceForUID:(NSString *)clockDeviceUID NS_SWIFT_NAME(clockDevice(_:));

@end

NS_ASSUME_NONNULL_END
