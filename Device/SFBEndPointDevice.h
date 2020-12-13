/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

NS_ASSUME_NONNULL_BEGIN

@class SFBAudioDevice;

/// An audio end point device
NS_SWIFT_NAME(EndPointDevice) @interface SFBEndPointDevice : SFBAudioDevice

/// Returns an array of available end point devices or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBEndPointDevice *> *endPointDevices;

/// Returns the end point device's composition \c nil on error
/// @note The constants for the dictionary keys are located in \c AudioHardwareBase.h
@property (nonatomic, nullable, readonly) NSDictionary * composition;
/// Returns an array of available end points or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *endPoints;
/// Returns the owning process id or \c 0 for public devices
@property (nonatomic, readonly) pid_t isPrivate;

@end

NS_ASSUME_NONNULL_END
