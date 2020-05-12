/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

#import "SFBAudioDeviceDataSource.h"

@class SFBAudioOutputDevice;

NS_ASSUME_NONNULL_BEGIN

/// Posted when the available audio devices change
extern const NSNotificationName SFBAudioDevicesChangedNotification;

/// An audio device with an underlying \c AudioObjectID supporting input or output
NS_SWIFT_NAME(AudioDevice) @interface SFBAudioDevice : NSObject

/// Returns an array of all available audio devices or \c nil on error
/// @note This returns \c { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementWildcard }
@property (class, nonatomic, readonly) NSArray<SFBAudioDevice *> *allDevices;
/// Returns an array of available audio devices supporting output or \c nil on error
/// @note A device supports output if it has a buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementWildcard }
@property (class, nonatomic, readonly) NSArray<SFBAudioOutputDevice *> *outputDevices;

/// Returns the default output device
@property (class, nonatomic, readonly) SFBAudioOutputDevice *defaultOutputDevice;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioDevice object with the specified device UID
/// @param deviceUID The desired device UID
/// @return An initialized \c SFBAudioDevice object or \c nil if \c deviceUID is invalid or unknown
- (nullable instancetype)initWithDeviceUID:(NSString *)deviceUID;
/// Returns an initialized \c SFBAudioDevice object with the specified audio object ID
/// @param audioObjectID The desired audio object ID
/// @return An initialized \c SFBAudioDevice object or \c nil if \c audioObjectID is invalid or unknown
- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID NS_DESIGNATED_INITIALIZER;

/// Returns the device ID
@property (nonatomic, readonly) AudioObjectID deviceID;
/// Returns the device UID
@property (nonatomic, readonly) NSString *deviceUID;
/// Returns the device name
@property (nonatomic, nullable, readonly) NSString *name;
/// Returns the device manufacturer
@property (nonatomic, nullable, readonly) NSString *manufacturer;

/// Returns \c YES if the device supports input
/// @note A device supports input if it has a buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementWildcard }
@property (nonatomic, readonly) BOOL supportsInput;
/// Returns \c YES if the device supports output
/// @note A device supports output if it has a buffers in {kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementWildcard }
@property (nonatomic, readonly) BOOL supportsOutput;

#pragma mark - Device Properties

/// Returns the device sample rate or \c NaN on error
/// @note This returns \c { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
@property (nonatomic, readonly) double sampleRate;
/// Sets the device sample rate
/// @note This sets \c { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param sampleRate The desired sample rate
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the sample rate was set successfully
- (BOOL)setSampleRate:(double)sampleRate error:(NSError **)error;

/// Returns an array of available sample rates
/// @note This returns \c { kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
@property (nonatomic, readonly) NSArray<NSNumber *> *availableSampleRates NS_REFINED_FOR_SWIFT;

/// Returns the volume scalar of the specified channel or \c NaN on error
/// @note This returns \c { kAudioDevicePropertyVolumeScalar, scope, channel }
/// @param channel The desired channel
/// @param scope The desired scope
/// @return The volume scalar of the specified channel or \c NaN on error
- (float)volumeForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope;
/// Sets the volume scalar of the specified channel
/// @note This sets \c { kAudioDevicePropertyVolumeScalar, scope, channel }
/// @param volume The desired volume scalar
/// @param channel The desired channel
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the volume was set successfully
- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope error:(NSError **)error;

/// Returns the volume in decibels of the specified channel or \c NaN on error
/// @note This returns \c { kAudioDevicePropertyVolumeDecibels, scope, channel }
/// @param channel The desired channel
/// @param scope The desired scope
/// @return The volume of the specified channel or \c NaN on error
- (float)volumeInDecibelsForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope;
/// Sets the volume in decibels of the specified channel
/// @note This sets \c { kAudioDevicePropertyVolumeDecibels, scope, channel }
/// @param volumeInDecibels The desired volume in decibels
/// @param channel The desired channel
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the volume was set successfully
- (BOOL)setVolumeInDecibels:(float)volumeInDecibels forChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope error:(NSError **)error;

/// Converts a volume scalar to a volume in decibels
/// @note This is the transformation performed by \c { kAudioDevicePropertyVolumeScalarToDecibels, scope, kAudioObjectPropertyElementMaster }
/// @param volumeScalar The volume scalar to convert
/// @param scope The desired scope
/// @return The volume in decibels for the volume scalar or \c NaN on error
- (float)convertVolumeScalar:(float)volumeScalar toDecibelsInScope:(AudioObjectPropertyScope)scope;
/// Converts a volume in decibels to scalar
/// @note This is the transformation performed by \c { kAudioDevicePropertyVolumeScalarToDecibels, scope, kAudioObjectPropertyElementMaster }
/// @param decibels The volume in decibels to convert
/// @param scope The desired scope
/// @return The volume scalar for the volume in decibels or \c NaN on error
- (float)convertDecibels:(float)decibels toVolumeScalarInScope:(AudioObjectPropertyScope)scope;

/// Returns an array of \c SFBAudioDeviceDataSource objects for the specified scope
/// @note This returns \c { kAudioDevicePropertyDataSources, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return An array containing the data sources or \c nil on error
- (NSArray<SFBAudioDeviceDataSource *> *)dataSourcesInScope:(AudioObjectPropertyScope)scope;

/// Returns an array of active \c SFBAudioDeviceDataSource objects for the specified scope
/// @note This returns \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return An array containing the active data sources or \c nil on error
- (NSArray<SFBAudioDeviceDataSource *> *)activeDataSourcesInScope:(AudioObjectPropertyScope)scope;
/// Sets the active data sources for the specified scope
/// @note This sets \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param activeDataSources An array of \c SFBAudioDeviceDataSource objects to make active for the specified scope
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the data sources were set successfully
- (BOOL)setActiveDataSources:(NSArray<SFBAudioDeviceDataSource *> *)activeDataSources inScope:(AudioObjectPropertyScope)scope error:(NSError **)error;

#pragma mark - Device Property Observation

/// Performs a block when the device sample rate changes
/// @note This observes \c { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param block A block to invoke when the sample rate changes or \c nil to remove the previous value
- (void)whenSampleRateChangesPerformBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenSampleRateChanges(perform:));
/// Performs a block when the device data sources in a scope change
/// @note This observes \c { kAudioDevicePropertyDataSources, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @param block A block to invoke when the data sources change or \c nil to remove the previous value
- (void)whenDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the active device data sources in a scope change
/// @note This observes \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @param block A block to invoke when the active data sources change or \c nil to remove the previous value
- (void)whenActiveDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;

/// Returns \c YES if the underlying audio object has the specified property
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property;
/// Returns \c YES if the underlying audio object has the specified property in a scope
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope;
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Performs a block when the specified property changes
/// @note This observes \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenPropertyChanges:(AudioObjectPropertySelector)property performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the specified property in a scope changes
/// @note This observes \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param scope The desired scope
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(AudioObjectPropertySelector)property changesInScope:(AudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope changesOnElement:(AudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block;

@end

NS_ASSUME_NONNULL_END
