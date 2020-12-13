/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio end point device
NS_SWIFT_NAME(EndPointDevice) @interface SFBEndPointDevice : SFBAudioDevice

/// Returns an array of available end point devices or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBEndPointDevice *> *endPointDevices;

@end

NS_ASSUME_NONNULL_END
