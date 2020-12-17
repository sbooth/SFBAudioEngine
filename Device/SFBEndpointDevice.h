/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio endpoint device
NS_SWIFT_NAME(EndpointDevice) @interface SFBEndpointDevice : SFBAudioDevice

/// Returns an array of available endpoint devices or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBEndpointDevice *> *endpointDevices;

/// Returns the endpoint device's composition \c nil on error
/// @note This corresponds to \c kAudioEndPointDevicePropertyComposition
/// @note The constants for the dictionary keys are located in \c AudioHardwareBase.h
@property (nonatomic, nullable, readonly) NSDictionary * composition NS_REFINED_FOR_SWIFT;
/// Returns an array of available endpoints or \c nil on error
/// @note This corresponds to \c kAudioEndPointDevicePropertyEndPointList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *endpoints NS_REFINED_FOR_SWIFT;
/// Returns the owning \c pid_t (\c 0 for public devices) or \c nil on error
/// @note This corresponds to \c kAudioEndPointDevicePropertyIsPrivate
@property (nonatomic, nullable, readonly) NSNumber * isPrivate NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
