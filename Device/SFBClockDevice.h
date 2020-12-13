/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio clock device
NS_SWIFT_NAME(ClockDevice) @interface SFBClockDevice : SFBAudioObject

/// Returns an array of available clock devices or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBClockDevice *> *clockDevices;

/// Returns an initialized \c SFBAudioClockDevice object with the specified clock UID
/// @param clockDeviceUID The desired clock UID
/// @return An initialized \c SFBAudioClockDevice object or \c nil if \c clockDeviceUID is invalid or unknown
- (nullable instancetype)initWithClockDeviceUID:(NSString *)clockDeviceUID;

/// Returns the clock device ID
/// @note This is equivalent to \c objectID
@property (nonatomic, readonly) AudioObjectID clockDeviceID;
/// Returns the clock device UID or \c nil on error
@property (nonatomic, nullable, readonly) NSString *clockDeviceUID;

@end

NS_ASSUME_NONNULL_END
