/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioDevice.h>

@class SFBClockDevice;

NS_ASSUME_NONNULL_BEGIN

/// An aggregate audio device
NS_SWIFT_NAME(AggregateDevice) @interface SFBAggregateDevice : SFBAudioDevice

/// Returns an array of available aggregate devices or \c nil on error
/// @note A device is an aggregate if its \c AudioClassID is \c kAudioAggregateDeviceClassID
@property (class, nonatomic, nullable, readonly) NSArray<SFBAggregateDevice *> *aggregateDevices;

/// Returns the UIDs of all subdevices in the aggregate device, active or inactive, or \c nil on error
/// @note This corresponds to \c kAudioAggregateDevicePropertyFullSubDeviceList
@property (nonatomic, nullable, readonly) NSArray <NSString *> *allSubdevicesIn NS_REFINED_FOR_SWIFT;
/// Returns the active subdevices in the aggregate device or \c nil on error
/// @note This corresponds to \c kAudioAggregateDevicePropertyActiveSubDeviceList
@property (nonatomic, nullable, readonly) NSArray <SFBAudioDevice *> *activeSubdevices NS_REFINED_FOR_SWIFT;

/// Returns the aggregate device's composition \c nil on error
/// @note This corresponds to \c kAudioAggregateDevicePropertyComposition
/// @note The constants for the dictionary keys are located in \c AudioHardware.h
@property (nonatomic, nullable, readonly) NSDictionary *composition NS_REFINED_FOR_SWIFT;

/// Returns the aggregate device's master subdevice or \c nil on error
/// @note This corresponds to \c kAudioAggregateDevicePropertyMasterSubDevice
@property (nonatomic, nullable, readonly) SFBAudioDevice *masterSubdevice NS_REFINED_FOR_SWIFT;

/// The aggregate device's clock device or \c nil if none
/// @note This corresponds to \c kAudioAggregateDevicePropertyClockDevice
@property (nonatomic, nullable, readonly) SFBClockDevice *clockDevice NS_REFINED_FOR_SWIFT;
/// Sets the aggregate device's clock device
/// @note This corresponds to \c kAudioAggregateDevicePropertyClockDevice
/// @param clockDevice The desired clock device
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the clock device was set successfully
- (BOOL)setClockDevice:(SFBClockDevice *)clockDevice error:(NSError **)error NS_REFINED_FOR_SWIFT;

#pragma mark - Convenience Accessors

/// Returns \c YES if the aggregate device is private
/// @note This returns the value of \c kAudioAggregateDeviceIsPrivateKey from \c self.composition
@property (nonatomic, nullable, readonly) NSNumber *isPrivate NS_REFINED_FOR_SWIFT;

/// Returns \c YES if the aggregate device is stacked
/// @note This returns the value of \c kAudioAggregateDeviceIsStackedKey from \c self.composition
@property (nonatomic, nullable, readonly) NSNumber *isStacked NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
