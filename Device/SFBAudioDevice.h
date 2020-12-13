/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>
#import <AVFoundation/AVFoundation.h>

@class SFBAudioStream, SFBAudioDeviceDataSource;

NS_ASSUME_NONNULL_BEGIN

/// Posted when the available audio devices change
extern const NSNotificationName SFBAudioDevicesChangedNotification;

/// An audio device supporting input and/or output
NS_SWIFT_NAME(AudioDevice) @interface SFBAudioDevice : SFBAudioObject

/// Returns an array of available audio devices or \c nil on error
/// @note This returns \c { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *devices;

/// Returns an array of available audio devices supporting input or \c nil on error
/// @note A device supports input if it has a buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMaster }
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *inputDevices;

/// Returns an array of available audio devices supporting output or \c nil on error
/// @note A device supports output if it has a buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *outputDevices;

/// Returns the default input device or \c nil on error
@property (class, nonatomic, nullable, readonly) SFBAudioDevice *defaultInputDevice;

/// Returns the default output device or \c nil on error
@property (class, nonatomic, nullable, readonly) SFBAudioDevice *defaultOutputDevice;

/// Returns the default system output device or \c nil on error
@property (class, nonatomic, nullable, readonly) SFBAudioDevice *defaultSystemOutputDevice;

/// Returns an initialized \c SFBAudioDevice object with the specified device UID
/// @param deviceUID The desired device UID
/// @return An initialized \c SFBAudioDevice object or \c nil if \c deviceUID is invalid or unknown
- (nullable instancetype)initWithDeviceUID:(NSString *)deviceUID;

/// Returns \c YES if the device supports input
/// @note A device supports input if it has buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMaster }
@property (nonatomic, readonly) BOOL supportsInput;
/// Returns \c YES if the device supports output
/// @note A device supports output if it has buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
@property (nonatomic, readonly) BOOL supportsOutput;

/// Returns \c YES if the device is an aggregate device
/// @note A device is an aggregate if its \c AudioClassID is \c kAudioAggregateDeviceClassID
@property (nonatomic, readonly) BOOL isAggregate;
/// Returns \c YES if the device is a private aggregate device
/// @note An aggregate device is private if \c kAudioAggregateDeviceIsPrivateKey is true
@property (nonatomic, readonly) BOOL isPrivateAggregate;

/// Returns \c YES if the device is an end point device
/// @note A device is an aggregate if its \c AudioClassID is \c kAudioEndPointDeviceClassID
@property (nonatomic, readonly) BOOL isEndPoint;

#pragma mark - Device Properties

/// Returns the configuration application or \c nil on error
@property (nonatomic, nullable, readonly) NSString *configurationApplication;
/// Returns the device UID or \c nil on error
@property (nonatomic, nullable, readonly) NSString *deviceUID;
/// Returns the model UID or \c nil on error
@property (nonatomic, nullable, readonly) NSString *modelUID;
/// Returns the transport type  or \c 0 on error
@property (nonatomic, readonly) UInt32 transportType;
/// Returns an array  of related audio devices or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *relatedDevices;
/// Returns the clock domain  or \c 0 on error
@property (nonatomic, readonly) UInt32 clockDomain;
/// Returns \c YES if the device is alive
@property (nonatomic, readonly) BOOL isAlive;
/// Returns \c YES if the device is running
@property (nonatomic, readonly) BOOL isRunning;
/// Returns \c YES if the device can be the default device
@property (nonatomic, readonly) BOOL canBeDefault;
/// Returns \c YES if the device can be the system default device
@property (nonatomic, readonly) BOOL canBeSystemDefault;
/// Returns the latency  or \c 0 on error
@property (nonatomic, readonly) UInt32 latency;
/// Returns an array  of the device's audio streams or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBAudioStream *> *streams;
/// Returns an array  of the device's audio controls or \c nil on error
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *controls;
/// Returns the safety offset  or \c 0 on error
@property (nonatomic, readonly) UInt32 safetyOffset;

/// Returns the device sample rate or \c NaN on error
/// @note This returns \c { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
@property (nonatomic, readonly) double sampleRate;
/// Sets the device sample rate
/// @note This sets \c { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param sampleRate The desired sample rate
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the sample rate was set successfully
- (BOOL)setSampleRate:(double)sampleRate error:(NSError **)error;

/// Returns an array of available sample rates or \c nil on error
/// @note This returns \c { kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
@property (nonatomic, nullable, readonly) NSArray<NSNumber *> *availableSampleRates NS_REFINED_FOR_SWIFT;

/// Returns the URL of the device's icon or \c nil on error
@property (nonatomic, nullable, readonly) NSURL *icon;
/// Returns \c YES if the device is hidden
@property (nonatomic, readonly) BOOL isHidden;

/// Returns the preferred stereo channels for the device
/// @note This is the property \c { kAudioDevicePropertyPreferredChannelsForStereo, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return The preferred stereo channels or \c nil on error
- (nullable NSArray<NSNumber *> *)preferredStereoChannelsInScope:(AudioObjectPropertyScope)scope NS_REFINED_FOR_SWIFT;

/// Returns the preferred channel layout for the device
/// @note This corresponds to the property \c { kAudioDevicePropertyPreferredChannelLayout, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return The preferred channel layout or \c nil on error
- (nullable AVAudioChannelLayout *)preferredChannelLayoutInScope:(AudioObjectPropertyScope)scope;

#pragma mark - Audio Controls

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

/// Returns \c YES if the device is muted
/// @note This is the property \c { kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMaster }
- (BOOL)isMutedInScope:(AudioObjectPropertyScope)scope;
/// Mutes or unmutes the device
/// @note This sets \c { kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMaster }
- (BOOL)setMute:(BOOL)mute inScope:(AudioObjectPropertyScope)scope error:(NSError **)error;

/// Returns an array of \c SFBAudioDeviceDataSource objects for the specified scope
/// @note This returns \c { kAudioDevicePropertyDataSources, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return An array containing the data sources or \c nil on error
- (nullable NSArray<SFBAudioDeviceDataSource *> *)dataSourcesInScope:(AudioObjectPropertyScope)scope;

/// Returns an array of active \c SFBAudioDeviceDataSource objects for the specified scope
/// @note This returns \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return An array containing the active data sources or \c nil on error
- (nullable NSArray<SFBAudioDeviceDataSource *> *)activeDataSourcesInScope:(AudioObjectPropertyScope)scope;
/// Sets the active data sources for the specified scope
/// @note This sets \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param activeDataSources An array of \c SFBAudioDeviceDataSource objects to make active for the specified scope
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the data sources were set successfully
- (BOOL)setActiveDataSources:(NSArray<SFBAudioDeviceDataSource *> *)activeDataSources inScope:(AudioObjectPropertyScope)scope error:(NSError **)error;

/// Returns \c YES if the device is hogged for the specified scope
/// @note This queries \c { kAudioDevicePropertyHogMode, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return \c YES if the device is hogged or \c NO if the device is not hogged or an error occurs
- (BOOL)isHoggedInScope:(AudioObjectPropertyScope)scope;
/// Returns \c YES if the device is hogged for the specified scope and the current process is the owner
/// @note This queries \c { kAudioDevicePropertyHogMode, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return \c YES if the device is hogged and the current process is the owner or \c NO if the device is not hogged or an error occurs
- (BOOL)isHogOwnerInScope:(AudioObjectPropertyScope)scope;
/// Takes hog mode for the specified scope
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if hog mode was taken successfully
- (BOOL)startHoggingInScope:(AudioObjectPropertyScope)scope error:(NSError **)error;
/// Releases hog mode for the specified scope
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if hog mode was released successfully
- (BOOL)stopHoggingInScope:(AudioObjectPropertyScope)scope error:(NSError **)error;

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
/// Performs a block when mute in a scope changes
/// @note This observes \c { kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @param block A block to invoke when mute change or \c nil to remove the previous value
- (void)whenMuteChangeInScope:(AudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the volume for a channel in a scope changes
/// @note This observes \c { kAudioDevicePropertyDataSources, scope, kAudioObjectPropertyElementMaster }
/// @param channel The desired channel
/// @param scope The desired scope
/// @param block A block to invoke when the volume on the specified channel changes or \c nil to remove the previous value
- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;

@end

NS_ASSUME_NONNULL_END
