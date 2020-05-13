/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDevice.h"

@class SFBAudioDeviceDataSource;

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(AudioOutputDevice) @interface SFBAudioOutputDevice: SFBAudioDevice

#pragma mark - Device Properties

/// Mutes or unmutes the output device
/// @note This is the property \c { kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
@property (nonatomic, getter=isMuted) BOOL mute;

/// Returns \c YES if the device has a master volume
/// @note This queries \c { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @return \c YES if the device has a master volume
@property (nonatomic, readonly) BOOL hasMasterVolume;
/// Returns the master volume scalar or \c NaN on error
/// @note This returns \c { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @return The master volume scalar or \c NaN on error
@property (nonatomic, readonly) float masterVolume;
/// Sets the master volume scalar
/// @note This sets \c { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @param masterVolume The desired master volume scalar
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the master volume was set successfully
- (BOOL)setMasterVolume:(float)masterVolume error:(NSError **)error;

/// Returns the volume scalar of the specified channel or \c NaN on error
/// @note This is the value returned by \c { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, channel }
/// @param channel The desired channel
/// @return The volume scalar of the specified channel or \c NaN on error
- (float)volumeForChannel:(AudioObjectPropertyElement)channel;
/// Sets the volume scalar of the specified channel
/// @note This sets \c { kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, channel }
/// @param volume The desired volume scalar
/// @param channel The desired channel
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the volume was set successfully
- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error;

/// Returns the preferred stereo channels for the device
/// @note This is the property \c { kAudioDevicePropertyPreferredChannelsForStereo, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *preferredStereoChannels NS_REFINED_FOR_SWIFT;

/// Returns an array of \c SFBAudioDeviceDataSource objects
/// @note This consists of all values returned by \c { kAudioDevicePropertyDataSources, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @return An array containing the data sources or \c nil on error
@property (nonatomic, readonly) NSArray<SFBAudioDeviceDataSource *> *dataSources;

/// Returns an array of active \c SFBAudioDeviceDataSource objects
/// @note This consists of all values returned by \c { kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @return An array containing the active data sources or \c nil on error
@property (nonatomic, readonly) NSArray<SFBAudioDeviceDataSource *> *activeDataSources;
/// Sets the active data sources
/// @note This consists of all values returned by \c { kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @param activeDataSources An array of \c SFBAudioDeviceDataSource objects to make active for the specified scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the data sources were set successfully
- (BOOL)setActiveDataSources:(NSArray<SFBAudioDeviceDataSource *> *)activeDataSources error:(NSError **)error;

#pragma mark - Device Property Observation

/// Performs a block when the device mute changes
/// @note This observes \c { kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @param block A block to invoke when the device mute changes or \c nil to remove the previous value
- (void)whenMuteChangesPerformBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenMuteChanges(perform:));
/// Performs a block when the device master volume changes
/// @note This observes \c { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param block A block to invoke when the device master volume changes or \c nil to remove the previous value
- (void)whenMasterVolumeChangesPerformBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenMasterVolumeChanges(perform:));

/// Performs a block when the volume for a channel changes
/// @note This observes \c { kAudioDevicePropertyDataSources, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @param channel The desired channel
/// @param block A block to invoke when the volume on the specified channel changes or \c nil to remove the previous value
- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the device data sources change
/// @note This observes \c { kAudioDevicePropertyDataSources, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @param block A block to invoke when the data sources change or \c nil to remove the previous value
- (void)whenDataSourcesChangePerformBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenDataSourcesChange(perform:));
/// Performs a block when the active device data sources change
/// @note This observes \c { kAudioDevicePropertyDataSource, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
/// @param block A block to invoke when the active data sources change or \c nil to remove the previous value
- (void)whenActiveDataSourcesChangePerformBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenActiveDataSourcesChange(perform:));

@end

NS_ASSUME_NONNULL_END
