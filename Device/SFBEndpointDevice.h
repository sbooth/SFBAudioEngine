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
/// @note The constants for the dictionary keys are located in \c AudioHardwareBase.h
@property (nonatomic, nullable, readonly) NSDictionary * composition;
/// Returns an array of available endpoints or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *endpoints;
/// Returns the owning process id or \c 0 for public devices
@property (nonatomic, readonly) pid_t isPrivate;

@end

NS_ASSUME_NONNULL_END
