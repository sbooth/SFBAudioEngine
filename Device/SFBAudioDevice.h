/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioObject.h>
#import <AVFoundation/AVFoundation.h>

@class SFBAudioStream, SFBAudioDeviceDataSource, SFBAudioControl;

NS_ASSUME_NONNULL_BEGIN

/// Posted when the available audio devices change
extern const NSNotificationName SFBAudioDevicesChangedNotification;

/// An audio device supporting input and/or output
/// @note This class has four scopes (\c kAudioObjectPropertyScopeGlobal, \c kAudioObjectPropertyScopeInput, \c kAudioObjectPropertyScopeOutput, and \c kAudioObjectPropertyScopePlayThrough), a master element (\c kAudioObjectPropertyElementMaster), and an element for each channel in each stream
NS_SWIFT_NAME(AudioDevice) @interface SFBAudioDevice : SFBAudioObject

/// Returns an array of available audio devices or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyDevices on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *devices;

/// Returns the default input device or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyDefaultInputDevice on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) SFBAudioDevice *defaultInputDevice;

/// Returns the default output device or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyDefaultOutputDevice on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) SFBAudioDevice *defaultOutputDevice;

/// Returns the default system output device or \c nil on error
/// @note This corresponds to \c kAudioHardwarePropertyDefaultSystemOutputDevice on the object \c kAudioObjectSystemObject
@property (class, nonatomic, nullable, readonly) SFBAudioDevice *defaultSystemOutputDevice;

/// Returns an array of available audio devices supporting input or \c nil on error
/// @note A device supports input if it has buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMaster }
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *inputDevices;

/// Returns an array of available audio devices supporting output or \c nil on error
/// @note A device supports output if it has buffers in \c { kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster }
@property (class, nonatomic, nullable, readonly) NSArray<SFBAudioDevice *> *outputDevices;

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
- (BOOL)isPrivateAggregateInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns \c YES if the device is an endpoint device
/// @note A device is an endpoint device if its \c AudioClassID is \c kAudioEndPointDeviceClassID
@property (nonatomic, readonly) BOOL isEndpointDevice;
/// Returns \c YES if the device is an endpoint
/// @note A device is an endpoint if its \c AudioClassID is \c kAudioEndPointClassID
@property (nonatomic, readonly) BOOL isEndpoint;

/// Returns \c YES if the device is a subdevice
/// @note A device is a subdevice if its \c AudioClassID is \c kAudioSubDeviceClassID
@property (nonatomic, readonly) BOOL isSubdevice;

#pragma mark - Device Base Properties

/// Returns the configuration application or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyConfigurationApplication
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSString *)configurationApplicationInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the device UID or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyDeviceUID
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSString *)deviceUIDInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the model UID or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyModelUID
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSString *)modelUIDInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the transport type  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyTransportType
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)transportTypeInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns an array  of related audio devices or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyRelatedDevices
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<SFBAudioDevice *> *)relatedDevicesInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the clock domain  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyClockDomain
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)clockDomainInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns \c @ YES if the device is alive  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyDeviceIsAlive
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)isAliveInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns \c @ YES if the device is running  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyDeviceIsRunning
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)isRunningInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns \c @ YES if the device can be the default device  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyDeviceCanBeDefaultDevice
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)canBeDefaultInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns \c @ YES if the device can be the system default device  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyDeviceCanBeDefaultSystemDevice
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)canBeSystemDefaultInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the latency  or \c nil on error  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyLatency
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)latencyInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns an array  of the device's audio streams or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyStreams
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<SFBAudioStream *> *)streamsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns an array  of the device's audio controls or \c nil on error
/// @note This corresponds to \c kAudioObjectPropertyControlList
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<SFBAudioControl *> *)controlsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns the safety offset  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertySafetyOffset
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)safetyOffsetInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the device sample rate or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyNominalSampleRate
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)sampleRateInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Sets the device sample rate
/// @note This corresponds to \c kAudioDevicePropertyNominalSampleRate
/// @param sampleRate The desired sample rate
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the sample rate was set successfully
- (BOOL)setSampleRate:(double)sampleRate inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns an array of available sample rates or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyAvailableNominalSampleRates
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<NSNumber *> *)availableSampleRatesInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the URL of the device's icon or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyIcon
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSURL *)iconInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Returns \c @ YES if the device is hidden  or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyIsHidden
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)isHiddenInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the preferred stereo channels for the device or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyPreferredChannelsForStereo
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<NSNumber *> *)preferredStereoChannelsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the preferred channel layout for the device or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyPreferredChannelLayout
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable AVAudioChannelLayout *)preferredChannelLayoutInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;
/// Sets the preferred channel layout for the device
/// @note This corresponds to \c kAudioDevicePropertyPreferredChannelLayout
/// @param channelLayout The desired channel layout
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the sample rate was set successfully
- (BOOL)setPreferredChannelLayout:(AVAudioChannelLayout *)channelLayout inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

#pragma mark - Device Properties

/// Returns any error codes loading the driver plugin or \c nil on error
/// @note This corresponds to \c kAudioDevicePropertyPlugIn
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)plugInInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns \c YES if the device is hogged for the specified scope
/// @note This queries \c { kAudioDevicePropertyHogMode, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return \c YES if the device is hogged or \c NO if the device is not hogged or an error occurs
- (BOOL)isHoggedInScope:(SFBAudioObjectPropertyScope)scope;
/// Returns \c YES if the device is hogged for the specified scope and the current process is the owner
/// @note This queries \c { kAudioDevicePropertyHogMode, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return \c YES if the device is hogged and the current process is the owner or \c NO if the device is not hogged or an error occurs
- (BOOL)isHogOwnerInScope:(SFBAudioObjectPropertyScope)scope;
/// Takes hog mode for the specified scope
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if hog mode was taken successfully
- (BOOL)startHoggingInScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error;
/// Releases hog mode for the specified scope
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if hog mode was released successfully
- (BOOL)stopHoggingInScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error;

#pragma mark - Audio Controls

/// Returns the volume scalar of the specified channel or \c NaN on error
/// @note This returns \c { kAudioDevicePropertyVolumeScalar, scope, channel }
/// @param channel The desired channel
/// @param scope The desired scope
/// @return The volume scalar of the specified channel or \c NaN on error
- (float)volumeForChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope;
/// Sets the volume scalar of the specified channel
/// @note This sets \c { kAudioDevicePropertyVolumeScalar, scope, channel }
/// @param volume The desired volume scalar
/// @param channel The desired channel
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the volume was set successfully
- (BOOL)setVolume:(float)volume forChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error;

/// Returns the volume in decibels of the specified channel or \c NaN on error
/// @note This returns \c { kAudioDevicePropertyVolumeDecibels, scope, channel }
/// @param channel The desired channel
/// @param scope The desired scope
/// @return The volume of the specified channel or \c NaN on error
- (float)volumeInDecibelsForChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope;
/// Sets the volume in decibels of the specified channel
/// @note This sets \c { kAudioDevicePropertyVolumeDecibels, scope, channel }
/// @param volumeInDecibels The desired volume in decibels
/// @param channel The desired channel
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the volume was set successfully
- (BOOL)setVolumeInDecibels:(float)volumeInDecibels forChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error;

/// Converts a volume scalar to a volume in decibels
/// @note This is the transformation performed by \c { kAudioDevicePropertyVolumeScalarToDecibels, scope, kAudioObjectPropertyElementMaster }
/// @param volumeScalar The volume scalar to convert
/// @param scope The desired scope
/// @return The volume in decibels for the volume scalar or \c NaN on error
- (float)convertVolumeScalar:(float)volumeScalar toDecibelsInScope:(SFBAudioObjectPropertyScope)scope;
/// Converts a volume in decibels to scalar
/// @note This is the transformation performed by \c { kAudioDevicePropertyVolumeScalarToDecibels, scope, kAudioObjectPropertyElementMaster }
/// @param decibels The volume in decibels to convert
/// @param scope The desired scope
/// @return The volume scalar for the volume in decibels or \c NaN on error
- (float)convertDecibels:(float)decibels toVolumeScalarInScope:(SFBAudioObjectPropertyScope)scope;

/// Returns \c YES if the device is muted
/// @note This is the property \c { kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMaster }
- (BOOL)isMutedInScope:(SFBAudioObjectPropertyScope)scope;
/// Mutes or unmutes the device
/// @note This sets \c { kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMaster }
- (BOOL)setMute:(BOOL)mute inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error;

/// Returns an array of \c SFBAudioDeviceDataSource objects for the specified scope
/// @note This returns \c { kAudioDevicePropertyDataSources, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return An array containing the data sources or \c nil on error
- (nullable NSArray<SFBAudioDeviceDataSource *> *)dataSourcesInScope:(SFBAudioObjectPropertyScope)scope;

/// Returns an array of active \c SFBAudioDeviceDataSource objects for the specified scope
/// @note This returns \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @return An array containing the active data sources or \c nil on error
- (nullable NSArray<SFBAudioDeviceDataSource *> *)activeDataSourcesInScope:(SFBAudioObjectPropertyScope)scope;
/// Sets the active data sources for the specified scope
/// @note This sets \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param activeDataSources An array of \c SFBAudioDeviceDataSource objects to make active for the specified scope
/// @param scope The desired scope
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the data sources were set successfully
- (BOOL)setActiveDataSources:(NSArray<SFBAudioDeviceDataSource *> *)activeDataSources inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error;

#pragma mark - Device Property Observation

/// Performs a block when the device sample rate changes
/// @note This observes \c { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param block A block to invoke when the sample rate changes or \c nil to remove the previous value
- (void)whenSampleRateChangesPerformBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenSampleRateChanges(perform:));
/// Performs a block when the device data sources in a scope change
/// @note This observes \c { kAudioDevicePropertyDataSources, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @param block A block to invoke when the data sources change or \c nil to remove the previous value
- (void)whenDataSourcesChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the active device data sources in a scope change
/// @note This observes \c { kAudioDevicePropertyDataSource, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @param block A block to invoke when the active data sources change or \c nil to remove the previous value
- (void)whenActiveDataSourcesChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when mute in a scope changes
/// @note This observes \c { kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMaster }
/// @param scope The desired scope
/// @param block A block to invoke when mute change or \c nil to remove the previous value
- (void)whenMuteChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the volume for a channel in a scope changes
/// @note This observes \c { kAudioDevicePropertyDataSources, scope, kAudioObjectPropertyElementMaster }
/// @param channel The desired channel
/// @param scope The desired scope
/// @param block A block to invoke when the volume on the specified channel changes or \c nil to remove the previous value
- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;

@end

NS_ASSUME_NONNULL_END
