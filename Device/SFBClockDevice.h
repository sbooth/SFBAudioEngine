/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio clock device
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(ClockDevice) @interface SFBClockDevice : SFBAudioObject

/// Returns an array of available clock devices or \c nil on error
@property (class, nonatomic, nullable, readonly) NSArray<SFBClockDevice *> *clockDevices;

/// Returns an initialized \c SFBAudioClockDevice object with the specified clock UID
/// @param clockDeviceUID The desired clock UID
/// @return An initialized \c SFBAudioClockDevice object or \c nil if \c clockDeviceUID is invalid or unknown
- (nullable instancetype)initWithClockDeviceUID:(NSString *)clockDeviceUID;

/// Returns the clock device UID or \c nil on error
@property (nonatomic, nullable, readonly) NSString *clockDeviceUID;
/// Returns the transport type  or \c 0 on error
@property (nonatomic, readonly) UInt32 transportType;
/// Returns the domain  or \c 0 on error
@property (nonatomic, readonly) UInt32 domain;
/// Returns \c YES if the clock device is alive
@property (nonatomic, readonly) BOOL isAlive;
/// Returns \c YES if the clock device is running
@property (nonatomic, readonly) BOOL isRunning;
/// Returns the latency  or \c 0 on error
@property (nonatomic, readonly) UInt32 latency;
/// Returns an array  of the clock device's audio controls or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *controls;
/// Returns the device sample rate or \c NaN on error
/// @note This returns \c { kAudioClockDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
@property (nonatomic, readonly) double sampleRate;
/// Returns an array of available sample rates or \c nil on error
/// @note This returns \c { kAudioClockDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *availableSampleRates NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
