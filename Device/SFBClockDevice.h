/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>

@class SFBAudioControl;

NS_ASSUME_NONNULL_BEGIN

/// An audio clock device
/// @note This class has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
NS_SWIFT_NAME(ClockDevice) @interface SFBClockDevice : SFBAudioObject

/// Returns an array of available clock devices or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyClockDeviceList on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) NSArray<SFBClockDevice *> *clockDevices;

/// Returns an initialized \c SFBAudioClockDevice object with the specified clock UID
/// @param clockDeviceUID The desired clock UID
/// @return An initialized \c SFBAudioClockDevice object or \c nil if \c clockDeviceUID is invalid or unknown
- (nullable instancetype)initWithClockDeviceUID:(NSString *)clockDeviceUID;

/// Returns the clock device UID or \c nil on error
/// @note This corresponds to \c kAudioClockDevicePropertyDeviceUID
@property (nonatomic, nullable, readonly) NSString *clockDeviceUID;
/// Returns the transport type  or \c 0 on error
/// @note This corresponds to \c kAudioClockDevicePropertyTransportType
@property (nonatomic, readonly) SFBAudioDeviceTransportType transportType;
/// Returns the domain  or \c 0 on error
/// @note This corresponds to \c kAudioClockDevicePropertyClockDomain
@property (nonatomic, readonly) UInt32 domain;
/// Returns \c YES if the clock device is alive
/// @note This corresponds to \c kAudioClockDevicePropertyDeviceIsAlive
@property (nonatomic, readonly) BOOL isAlive;
/// Returns \c YES if the clock device is running
/// @note This corresponds to \c kAudioClockDevicePropertyDeviceIsRunning
@property (nonatomic, readonly) BOOL isRunning;
/// Returns the latency  or \c 0 on error
/// @note This corresponds to \c kAudioClockDevicePropertyLatency
@property (nonatomic, readonly) UInt32 latency;
/// Returns an array  of the clock device's audio controls or \c nil on error
/// @note This corresponds to \c kAudioClockDevicePropertyControlList
@property (nonatomic, nullable, readonly) NSArray<SFBAudioControl *> *controls;
/// Returns the device sample rate or \c NaN on error
/// @note This corresponds to \c kAudioClockDevicePropertyNominalSampleRate
@property (nonatomic, readonly) double sampleRate;
/// Returns an array of available sample rates or \c nil on error
/// @note This corresponds to \c kAudioClockDevicePropertyAvailableNominalSampleRates
/// @note The return value contains an array of wrapped \c AudioValueRange structures
@property (nonatomic, nullable, readonly) NSArray<NSValue *> *availableSampleRates NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
