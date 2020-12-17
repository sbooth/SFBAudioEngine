/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

NS_ASSUME_NONNULL_BEGIN

/// Property selectors for \c SFBAudioObject and subclasses
/// @note These are interchangeable with \c AudioObjectPropertySelector but are typed for ease of use from Swift.
typedef NS_ENUM(AudioObjectPropertySelector, SFBAudioObjectPropertySelector) {

	// Selectors from AudioHardwareBase.h

	// AudioObject
	SFBAudioObjectPropertySelectorBaseClass 			= kAudioObjectPropertyBaseClass,
	SFBAudioObjectPropertySelectorClass 				= kAudioObjectPropertyClass,
	SFBAudioObjectPropertySelectorOwner 				= kAudioObjectPropertyOwner,
	SFBAudioObjectPropertySelectorName 					= kAudioObjectPropertyName,
	SFBAudioObjectPropertySelectorModelName				= kAudioObjectPropertyModelName,
	SFBAudioObjectPropertySelectorManufacturer 			= kAudioObjectPropertyManufacturer,
	SFBAudioObjectPropertySelectorElementName 			= kAudioObjectPropertyElementName,
	SFBAudioObjectPropertySelectorElementCategoryName 	= kAudioObjectPropertyElementCategoryName,
	SFBAudioObjectPropertySelectorElementNumberName 	= kAudioObjectPropertyElementNumberName,
	SFBAudioObjectPropertySelectorOwnedObjects 			= kAudioObjectPropertyOwnedObjects,
	SFBAudioObjectPropertySelectorIdentify 				= kAudioObjectPropertyIdentify,
	SFBAudioObjectPropertySelectorSerialNumber 			= kAudioObjectPropertySerialNumber,
	SFBAudioObjectPropertySelectorFirmwareVersion 		= kAudioObjectPropertyFirmwareVersion,

	// AudioPlugIn
	SFBAudioObjectPropertySelectorPlugInBundleID 					= kAudioPlugInPropertyBundleID,
	SFBAudioObjectPropertySelectorPlugInDeviceList 					= kAudioPlugInPropertyDeviceList,
	SFBAudioObjectPropertySelectorPlugInTranslateUIDToDevice 		= kAudioPlugInPropertyTranslateUIDToDevice,
	SFBAudioObjectPropertySelectorPlugInBoxList 					= kAudioPlugInPropertyBoxList,
	SFBAudioObjectPropertySelectorPlugInTranslateUIDToBox 			= kAudioPlugInPropertyTranslateUIDToBox,
	SFBAudioObjectPropertySelectorPlugInClockDeviceList 			= kAudioPlugInPropertyClockDeviceList,
	SFBAudioObjectPropertySelectorPlugInTranslateUIDToClockDevice 	= kAudioPlugInPropertyTranslateUIDToClockDevice,

	// AudioTransportManager
	SFBAudioObjectPropertySelectorTransportManagerEndPointList 				= kAudioTransportManagerPropertyEndPointList,
	SFBAudioObjectPropertySelectorTransportManagerTranslateUIDToEndPoint 	= kAudioTransportManagerPropertyTranslateUIDToEndPoint,
	SFBAudioObjectPropertySelectorTransportManagerTransportType 			= kAudioTransportManagerPropertyTransportType,

	// AudioBox
	SFBAudioObjectPropertySelectorBoxUID 				= kAudioBoxPropertyBoxUID,
	SFBAudioObjectPropertySelectorBoxTransportType 		= kAudioBoxPropertyTransportType,
	SFBAudioObjectPropertySelectorBoxHasAudio 			= kAudioBoxPropertyHasAudio,
	SFBAudioObjectPropertySelectorBoxHasVideo 			= kAudioBoxPropertyHasVideo,
	SFBAudioObjectPropertySelectorBoxHasMIDI 			= kAudioBoxPropertyHasMIDI,
	SFBAudioObjectPropertySelectorBoxIsProtected 		= kAudioBoxPropertyIsProtected,
	SFBAudioObjectPropertySelectorBoxAcquired 			= kAudioBoxPropertyAcquired,
	SFBAudioObjectPropertySelectorBoxAcquisitionFailed 	= kAudioBoxPropertyAcquisitionFailed,
	SFBAudioObjectPropertySelectorBoxDeviceList 		= kAudioBoxPropertyDeviceList,
	SFBAudioObjectPropertySelectorBoxClockDeviceList 	= kAudioBoxPropertyClockDeviceList,

	// AudioDevice
	SFBAudioObjectPropertySelectorDeviceConfigurationApplication 		= kAudioDevicePropertyConfigurationApplication,
	SFBAudioObjectPropertySelectorDeviceDeviceUID 						= kAudioDevicePropertyDeviceUID,
	SFBAudioObjectPropertySelectorDeviceModelUID 						= kAudioDevicePropertyModelUID,
	SFBAudioObjectPropertySelectorDeviceTransportType 					= kAudioDevicePropertyTransportType,
	SFBAudioObjectPropertySelectorDeviceRelatedDevices 					= kAudioDevicePropertyRelatedDevices,
	SFBAudioObjectPropertySelectorDeviceClockDomain 					= kAudioDevicePropertyClockDomain,
	SFBAudioObjectPropertySelectorDeviceDeviceIsAlive 					= kAudioDevicePropertyDeviceIsAlive,
	SFBAudioObjectPropertySelectorDeviceDeviceIsRunning 				= kAudioDevicePropertyDeviceIsRunning,
	SFBAudioObjectPropertySelectorDeviceDeviceCanBeDefaultDevice 		= kAudioDevicePropertyDeviceCanBeDefaultDevice,
	SFBAudioObjectPropertySelectorDeviceDeviceCanBeDefaultSystemDevice 	= kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
	SFBAudioObjectPropertySelectorDeviceLatency 						= kAudioDevicePropertyLatency,
	SFBAudioObjectPropertySelectorDeviceStreams 						= kAudioDevicePropertyStreams,
	SFBAudioObjectPropertySelectorControlList 							= kAudioObjectPropertyControlList,
	SFBAudioObjectPropertySelectorDeviceSafetyOffset 					= kAudioDevicePropertySafetyOffset,
	SFBAudioObjectPropertySelectorDeviceNominalSampleRate 				= kAudioDevicePropertyNominalSampleRate,
	SFBAudioObjectPropertySelectorDeviceAvailableNominalSampleRates 	= kAudioDevicePropertyAvailableNominalSampleRates,
	SFBAudioObjectPropertySelectorDeviceIcon 							= kAudioDevicePropertyIcon,
	SFBAudioObjectPropertySelectorDeviceIsHidden 						= kAudioDevicePropertyIsHidden,
	SFBAudioObjectPropertySelectorDevicePreferredChannelsForStereo 		= kAudioDevicePropertyPreferredChannelsForStereo,
	SFBAudioObjectPropertySelectorDevicePreferredChannelLayout 			= kAudioDevicePropertyPreferredChannelLayout,

	// AudioClockDevice
	SFBAudioObjectPropertySelectorClockDeviceDeviceUID 						= kAudioClockDevicePropertyDeviceUID,
	SFBAudioObjectPropertySelectorClockDeviceTransportType 					= kAudioClockDevicePropertyTransportType,
	SFBAudioObjectPropertySelectorClockDeviceClockDomain 					= kAudioClockDevicePropertyClockDomain,
	SFBAudioObjectPropertySelectorClockDeviceDeviceIsAlive 					= kAudioClockDevicePropertyDeviceIsAlive,
	SFBAudioObjectPropertySelectorClockDeviceDeviceIsRunning 				= kAudioClockDevicePropertyDeviceIsRunning,
	SFBAudioObjectPropertySelectorClockDeviceLatency 						= kAudioClockDevicePropertyLatency,
	SFBAudioObjectPropertySelectorClockDeviceControlList 					= kAudioClockDevicePropertyControlList,
	SFBAudioObjectPropertySelectorClockDeviceNominalSampleRate 				= kAudioClockDevicePropertyNominalSampleRate,
	SFBAudioObjectPropertySelectorClockDeviceAvailableNominalSampleRates 	= kAudioClockDevicePropertyAvailableNominalSampleRates,

	// AudioEndPointDevice
	SFBAudioObjectPropertySelectorEndpointDeviceComposition 	= kAudioEndPointDevicePropertyComposition,
	SFBAudioObjectPropertySelectorEndpointDeviceEndPointList 	= kAudioEndPointDevicePropertyEndPointList,
	SFBAudioObjectPropertySelectorEndpointDeviceIsPrivate 		= kAudioEndPointDevicePropertyIsPrivate,

	// AudioStream
	SFBAudioObjectPropertySelectorStreamIsActive 					= kAudioStreamPropertyIsActive,
	SFBAudioObjectPropertySelectorStreamDirection 					= kAudioStreamPropertyDirection,
	SFBAudioObjectPropertySelectorStreamTerminalType 				= kAudioStreamPropertyTerminalType,
	SFBAudioObjectPropertySelectorStreamStartingChannel 			= kAudioStreamPropertyStartingChannel,
	SFBAudioObjectPropertySelectorStreamLatency 					= kAudioStreamPropertyLatency,
	SFBAudioObjectPropertySelectorStreamVirtualFormat 				= kAudioStreamPropertyVirtualFormat,
	SFBAudioObjectPropertySelectorStreamAvailableVirtualFormats 	= kAudioStreamPropertyAvailableVirtualFormats,
	SFBAudioObjectPropertySelectorStreamPhysicalFormat 				= kAudioStreamPropertyPhysicalFormat,
	SFBAudioObjectPropertySelectorStreamAvailablePhysicalFormats 	= kAudioStreamPropertyAvailablePhysicalFormats,

	// AudioControl
	SFBAudioObjectPropertySelectorControlScope 		= kAudioControlPropertyScope,
	SFBAudioObjectPropertySelectorControlElement 	= kAudioControlPropertyElement,

	// AudioSliderControl
	SFBAudioObjectPropertySelectorSliderControlValue 	= kAudioSliderControlPropertyValue,
	SFBAudioObjectPropertySelectorSliderControlRange 	= kAudioSliderControlPropertyRange,

	// AudioLevelControl
	SFBAudioObjectPropertySelectorLevelControlScalarValue 		= kAudioLevelControlPropertyScalarValue,
	SFBAudioObjectPropertySelectorLevelControlDecibelValue 		= kAudioLevelControlPropertyDecibelValue,
	SFBAudioObjectPropertySelectorLevelControlDecibelRange 		= kAudioLevelControlPropertyDecibelRange,
	SFBAudioObjectPropertySelectorLevelControlScalarToDecibels 	= kAudioLevelControlPropertyConvertScalarToDecibels,
	SFBAudioObjectPropertySelectorLevelControlDecibelsToScalar 	= kAudioLevelControlPropertyConvertDecibelsToScalar,

	// AudioBooleanControl
	SFBAudioObjectPropertySelectorBooleanControlValue 	= kAudioBooleanControlPropertyValue,

	// AudioSelectorControl
	SFBAudioObjectPropertySelectorSelectorControlCurrentItem 		= kAudioSelectorControlPropertyCurrentItem,
	SFBAudioObjectPropertySelectorSelectorControlAvailableItems 	= kAudioSelectorControlPropertyAvailableItems,
	SFBAudioObjectPropertySelectorSelectorControlItemName 			= kAudioSelectorControlPropertyItemName,
	SFBAudioObjectPropertySelectorSelectorControlItemKind 			= kAudioSelectorControlPropertyItemKind,

	// AudioStereoPanControl
	SFBAudioObjectPropertySelectorStereoPanControlValue 			= kAudioStereoPanControlPropertyValue,
	SFBAudioObjectPropertySelectorStereoPanControlPanningChannels 	= kAudioStereoPanControlPropertyPanningChannels,

	/// Wildcard  selector, useful for notifications
	SFBAudioObjectPropertySelectorWildcard 	= kAudioObjectPropertySelectorWildcard,

	// Selectors from AudioHardware.h

	// AudioObject
	SFBAudioObjectPropertySelectorCreator 			= kAudioObjectPropertyCreator,
	SFBAudioObjectPropertySelectorListenerAdded 	= kAudioObjectPropertyListenerAdded,
	SFBAudioObjectPropertySelectorListenerRemoved 	= kAudioObjectPropertyListenerRemoved,

	// AudioSystemObject
	SFBAudioObjectPropertySelectorDevices 								= kAudioHardwarePropertyDevices,
	SFBAudioObjectPropertySelectorDefaultInputDevice 					= kAudioHardwarePropertyDefaultInputDevice,
	SFBAudioObjectPropertySelectorDefaultOutputDevice 					= kAudioHardwarePropertyDefaultOutputDevice,
	SFBAudioObjectPropertySelectorDefaultSystemOutputDevice 			= kAudioHardwarePropertyDefaultSystemOutputDevice,
	SFBAudioObjectPropertySelectorTranslateUIDToDevice 					= kAudioHardwarePropertyTranslateUIDToDevice,
	SFBAudioObjectPropertySelectorMixStereoToMono 						= kAudioHardwarePropertyMixStereoToMono,
	SFBAudioObjectPropertySelectorPlugInList 							= kAudioHardwarePropertyPlugInList,
	SFBAudioObjectPropertySelectorTranslateBundleIDToPlugIn 			= kAudioHardwarePropertyTranslateBundleIDToPlugIn,
	SFBAudioObjectPropertySelectorTransportManagerList 					= kAudioHardwarePropertyTransportManagerList,
	SFBAudioObjectPropertySelectorTranslateBundleIDToTransportManager 	= kAudioHardwarePropertyTranslateBundleIDToTransportManager,
	SFBAudioObjectPropertySelectorBoxList 								= kAudioHardwarePropertyBoxList,
	SFBAudioObjectPropertySelectorTranslateUIDToBox 					= kAudioHardwarePropertyTranslateUIDToBox,
	SFBAudioObjectPropertySelectorClockDeviceList 						= kAudioHardwarePropertyClockDeviceList,
	SFBAudioObjectPropertySelectorTranslateUIDToClockDevice 			= kAudioHardwarePropertyTranslateUIDToClockDevice,
	SFBAudioObjectPropertySelectorProcessIsMaster 						= kAudioHardwarePropertyProcessIsMaster,
	SFBAudioObjectPropertySelectorIsInitingOrExiting 					= kAudioHardwarePropertyIsInitingOrExiting,
	SFBAudioObjectPropertySelectorUserIDChanged 						= kAudioHardwarePropertyUserIDChanged,
	SFBAudioObjectPropertySelectorProcessIsAudible 						= kAudioHardwarePropertyProcessIsAudible,
	SFBAudioObjectPropertySelectorSleepingIsAllowed 					= kAudioHardwarePropertySleepingIsAllowed,
	SFBAudioObjectPropertySelectorUnloadingIsAllowed 					= kAudioHardwarePropertyUnloadingIsAllowed,
	SFBAudioObjectPropertySelectorHogModeIsAllowed 						= kAudioHardwarePropertyHogModeIsAllowed,
	SFBAudioObjectPropertySelectorUserSessionIsActiveOrHeadless 		= kAudioHardwarePropertyUserSessionIsActiveOrHeadless,
	SFBAudioObjectPropertySelectorServiceRestarted 						= kAudioHardwarePropertyServiceRestarted,
	SFBAudioObjectPropertySelectorPowerHint 							= kAudioHardwarePropertyPowerHint,

	// AudioPlugIn
	SFBAudioObjectPropertySelectorPlugInCreateAggregateDevice 	= kAudioPlugInCreateAggregateDevice,
	SFBAudioObjectPropertySelectorPlugInDestroyAggregateDevice 	= kAudioPlugInDestroyAggregateDevice,

	// AudioTransportManager
	SFBAudioObjectPropertySelectorTransportManagerCreateEndpointDevice 		= kAudioTransportManagerCreateEndPointDevice,
	SFBAudioObjectPropertySelectorTransportManagerDestroyEndpointDevice 	= kAudioTransportManagerDestroyEndPointDevice,

	// AudioDevice
	SFBAudioObjectPropertySelectorDevicePlugIn 							= kAudioDevicePropertyPlugIn,
	SFBAudioObjectPropertySelectorDeviceDeviceHasChanged 				= kAudioDevicePropertyDeviceHasChanged,
	SFBAudioObjectPropertySelectorDeviceDeviceIsRunningSomewhere 		= kAudioDevicePropertyDeviceIsRunningSomewhere,
	SFBAudioObjectPropertySelectorProcessorOverload 					= kAudioDeviceProcessorOverload,
	SFBAudioObjectPropertySelectorDeviceIOStoppedAbnormally 			= kAudioDevicePropertyIOStoppedAbnormally,
	SFBAudioObjectPropertySelectorDeviceHogMode 						= kAudioDevicePropertyHogMode,
	SFBAudioObjectPropertySelectorDeviceBufferFrameSize 				= kAudioDevicePropertyBufferFrameSize,
	SFBAudioObjectPropertySelectorDeviceBufferFrameSizeRange 			= kAudioDevicePropertyBufferFrameSizeRange,
	SFBAudioObjectPropertySelectorDeviceUsesVariableBufferFrameSizes 	= kAudioDevicePropertyUsesVariableBufferFrameSizes,
	SFBAudioObjectPropertySelectorDeviceIOCycleUsage 					= kAudioDevicePropertyIOCycleUsage,
	SFBAudioObjectPropertySelectorDeviceStreamConfiguration 			= kAudioDevicePropertyStreamConfiguration,
	SFBAudioObjectPropertySelectorDeviceIOProcStreamUsage 				= kAudioDevicePropertyIOProcStreamUsage,
	SFBAudioObjectPropertySelectorDeviceActualSampleRate 				= kAudioDevicePropertyActualSampleRate,
	SFBAudioObjectPropertySelectorDeviceClockDevice 					= kAudioDevicePropertyClockDevice,
	SFBAudioObjectPropertySelectorDeviceIOThreadOSWorkgroup 			= kAudioDevicePropertyIOThreadOSWorkgroup,

	SFBAudioObjectPropertySelectorDeviceJackIsConnected 							= kAudioDevicePropertyJackIsConnected,
	SFBAudioObjectPropertySelectorDeviceVolumeScalar 								= kAudioDevicePropertyVolumeScalar,
	SFBAudioObjectPropertySelectorDeviceVolumeDecibels 								= kAudioDevicePropertyVolumeDecibels,
	SFBAudioObjectPropertySelectorDeviceVolumeRangeDecibels 						= kAudioDevicePropertyVolumeRangeDecibels,
	SFBAudioObjectPropertySelectorDeviceVolumeScalarToDecibels 						= kAudioDevicePropertyVolumeScalarToDecibels,
	SFBAudioObjectPropertySelectorDeviceVolumeDecibelsToScalar 						= kAudioDevicePropertyVolumeDecibelsToScalar,
	SFBAudioObjectPropertySelectorDeviceStereoPan 									= kAudioDevicePropertyStereoPan,
	SFBAudioObjectPropertySelectorDeviceStereoPanChannels 							= kAudioDevicePropertyStereoPanChannels,
	SFBAudioObjectPropertySelectorDeviceMute 										= kAudioDevicePropertyMute,
	SFBAudioObjectPropertySelectorDeviceSolo 										= kAudioDevicePropertySolo,
	SFBAudioObjectPropertySelectorDevicePhantomPower 								= kAudioDevicePropertyPhantomPower,
	SFBAudioObjectPropertySelectorDevicePhaseInvert 								= kAudioDevicePropertyPhaseInvert,
	SFBAudioObjectPropertySelectorDeviceClipLight 									= kAudioDevicePropertyClipLight,
	SFBAudioObjectPropertySelectorDeviceTalkback 									= kAudioDevicePropertyTalkback,
	SFBAudioObjectPropertySelectorDeviceListenback 									= kAudioDevicePropertyListenback,
	SFBAudioObjectPropertySelectorDeviceDataSource 									= kAudioDevicePropertyDataSource,
	SFBAudioObjectPropertySelectorDeviceDataSources 								= kAudioDevicePropertyDataSources,
	SFBAudioObjectPropertySelectorDeviceDataSourceNameForIDCFString 				= kAudioDevicePropertyDataSourceNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceDataSourceKindForID 						= kAudioDevicePropertyDataSourceKindForID,
	SFBAudioObjectPropertySelectorDeviceClockSource 								= kAudioDevicePropertyClockSource,
	SFBAudioObjectPropertySelectorDeviceClockSources 								= kAudioDevicePropertyClockSources,
	SFBAudioObjectPropertySelectorDeviceClockSourceNameForIDCFString 				= kAudioDevicePropertyClockSourceNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceClockSourceKindForID 						= kAudioDevicePropertyClockSourceKindForID,
	SFBAudioObjectPropertySelectorDevicePlayThru 									= kAudioDevicePropertyPlayThru,
	SFBAudioObjectPropertySelectorDevicePlayThruSolo 								= kAudioDevicePropertyPlayThruSolo,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeScalar 						= kAudioDevicePropertyPlayThruVolumeScalar,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeDecibels 						= kAudioDevicePropertyPlayThruVolumeDecibels,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeRangeDecibels 				= kAudioDevicePropertyPlayThruVolumeRangeDecibels,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeScalarToDecibels 				= kAudioDevicePropertyPlayThruVolumeScalarToDecibels,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeDecibelsToScalar 				= kAudioDevicePropertyPlayThruVolumeDecibelsToScalar,
	SFBAudioObjectPropertySelectorDevicePlayThruStereoPan 							= kAudioDevicePropertyPlayThruStereoPan,
	SFBAudioObjectPropertySelectorDevicePlayThruStereoPanChannels 					= kAudioDevicePropertyPlayThruStereoPanChannels,
	SFBAudioObjectPropertySelectorDevicePlayThruDestination 						= kAudioDevicePropertyPlayThruDestination,
	SFBAudioObjectPropertySelectorDevicePlayThruDestinations 						= kAudioDevicePropertyPlayThruDestinations,
	SFBAudioObjectPropertySelectorDevicePlayThruDestinationNameForIDCFString 		= kAudioDevicePropertyPlayThruDestinationNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceChannelNominalLineLevel 					= kAudioDevicePropertyChannelNominalLineLevel,
	SFBAudioObjectPropertySelectorDeviceChannelNominalLineLevels 					= kAudioDevicePropertyChannelNominalLineLevels,
	SFBAudioObjectPropertySelectorDeviceChannelNominalLineLevelNameForIDCFString 	= kAudioDevicePropertyChannelNominalLineLevelNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceHighPassFilterSetting 						= kAudioDevicePropertyHighPassFilterSetting,
	SFBAudioObjectPropertySelectorDeviceHighPassFilterSettings 						= kAudioDevicePropertyHighPassFilterSettings,
	SFBAudioObjectPropertySelectorDeviceHighPassFilterSettingNameForIDCFString 		= kAudioDevicePropertyHighPassFilterSettingNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceSubVolumeScalar 							= kAudioDevicePropertySubVolumeScalar,
	SFBAudioObjectPropertySelectorDeviceSubVolumeDecibels 							= kAudioDevicePropertySubVolumeDecibels,
	SFBAudioObjectPropertySelectorDeviceSubVolumeRangeDecibels 						= kAudioDevicePropertySubVolumeRangeDecibels,
	SFBAudioObjectPropertySelectorDeviceSubVolumeScalarToDecibels 					= kAudioDevicePropertySubVolumeScalarToDecibels,
	SFBAudioObjectPropertySelectorDeviceSubVolumeDecibelsToScalar 					= kAudioDevicePropertySubVolumeDecibelsToScalar,
	SFBAudioObjectPropertySelectorDeviceSubMute 									= kAudioDevicePropertySubMute,

	// AudioAggregateDevice
	SFBAudioObjectPropertySelectorAggregateDeviceFullSubDeviceList 		= kAudioAggregateDevicePropertyFullSubDeviceList,
	SFBAudioObjectPropertySelectorAggregateDeviceActiveSubDeviceList 	= kAudioAggregateDevicePropertyActiveSubDeviceList,
	SFBAudioObjectPropertySelectorAggregateDeviceComposition 			= kAudioAggregateDevicePropertyComposition,
	SFBAudioObjectPropertySelectorAggregateDeviceMasterSubDevice 		= kAudioAggregateDevicePropertyMasterSubDevice,
	SFBAudioObjectPropertySelectorAggregateDeviceClockDevice 			= kAudioAggregateDevicePropertyClockDevice,

	// AudioSubDevice
	SFBAudioObjectPropertySelectorSubdeviceExtraLatency 				= kAudioSubDevicePropertyExtraLatency,
	SFBAudioObjectPropertySelectorSubdeviceDriftCompensation 			= kAudioSubDevicePropertyDriftCompensation,
	SFBAudioObjectPropertySelectorSubdeviceDriftCompensationQuality 	= kAudioSubDevicePropertyDriftCompensationQuality

} NS_SWIFT_NAME(AudioObject.PropertySelector);

/// Property scopes for \c SFBAudioObject and subclasses
/// @note These are interchangeable with \c AudioObjectPropertyScope but are typed for ease of use from Swift.
typedef NS_ENUM(AudioObjectPropertyScope, SFBAudioObjectPropertyScope) {
	/// Global scope
	SFBAudioObjectPropertyScopeGlobal		= kAudioObjectPropertyScopeGlobal,
	/// Input scope
	SFBAudioObjectPropertyScopeInput		= kAudioObjectPropertyScopeInput,
	/// Output scope
	SFBAudioObjectPropertyScopeOutput 		= kAudioObjectPropertyScopeOutput,
	/// Playthrough scope
	SFBAudioObjectPropertyScopePlayThrough 	= kAudioObjectPropertyScopePlayThrough,
	/// Wildcard  scope, useful for notifications
	SFBAudioObjectPropertyScopeWildcard		= kAudioObjectPropertyScopeWildcard
} NS_SWIFT_NAME(AudioObject.PropertyScope);

/// Audio device transport types
typedef NS_ENUM(UInt32, SFBAudioDeviceTransportType) {
	/// Unknown
	SFBAudioDeviceTransportTypeUnknown 		= kAudioDeviceTransportTypeUnknown,
	/// Built-in
	SFBAudioDeviceTransportTypeBuiltIn 		= kAudioDeviceTransportTypeBuiltIn,
	/// Aggregate device
	SFBAudioDeviceTransportTypeAggregate 	= kAudioDeviceTransportTypeAggregate,
	/// Virtual device
	SFBAudioDeviceTransportTypeVirtual 		= kAudioDeviceTransportTypeVirtual,
	/// PCI
	SFBAudioDeviceTransportTypePCI 			= kAudioDeviceTransportTypePCI,
	/// USB
	SFBAudioDeviceTransportTypeUSB 			= kAudioDeviceTransportTypeUSB,
	/// FireWire
	SFBAudioDeviceTransportTypeFireWire 	= kAudioDeviceTransportTypeFireWire,
	/// Bluetooth
	SFBAudioDeviceTransportTypeBluetooth 	= kAudioDeviceTransportTypeBluetooth,
	/// Bluetooth Low Energy
	SFBAudioDeviceTransportTypeBluetoothLE 	= kAudioDeviceTransportTypeBluetoothLE,
	/// HDMI
	SFBAudioDeviceTransportTypeHDMI 		= kAudioDeviceTransportTypeHDMI,
	/// DisplayPort
	SFBAudioDeviceTransportTypeDisplayPort 	= kAudioDeviceTransportTypeDisplayPort,
	/// AirPlay
	SFBAudioDeviceTransportTypeAirPlay 		= kAudioDeviceTransportTypeAirPlay,
	/// AVB
	SFBAudioDeviceTransportTypeAVB 			= kAudioDeviceTransportTypeAVB,
	/// Thunderbolt
	SFBAudioDeviceTransportTypeThunderbolt 	= kAudioDeviceTransportTypeThunderbolt
} NS_SWIFT_NAME(AudioDevice.TransportType);

/// Property element for \c SFBAudioObject and subclasses
/// @note This is interchangeable with \c AudioObjectPropertyElement but is typed for ease of use from Swift.
typedef AudioObjectPropertyElement SFBAudioObjectPropertyElement NS_SWIFT_NAME(AudioObject.PropertyElement);

/// An audio object
NS_SWIFT_NAME(AudioObject) @interface SFBAudioObject : NSObject

/// The singleton system audio object
/// @note This object has a single scope (\c kAudioObjectPropertyScopeGlobal) and a single element (\c kAudioObjectPropertyElementMaster)
+ (SFBAudioObject *)systemObject;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioObject object with the specified audio object ID
/// @note This returns a specialized subclass of \c SFBAudioObject when possible
/// @param objectID The desired audio object ID
/// @return An initialized \c SFBAudioObject object or \c nil if \c objectID is invalid or unknown
- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)objectID NS_DESIGNATED_INITIALIZER;

/// Returns the audio object's ID
@property (nonatomic, readonly) AudioObjectID objectID;

#pragma mark - Audio Object Property Information

/// Returns \c YES if the underlying audio object has the specified property
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -hasProperty:inScope:onElement:");
/// Returns \c YES if the underlying audio object has the specified property in a scope
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -hasProperty:inScope:onElement:");
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns \c YES if the underlying audio object has the specified property and it is settable
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -propertyIsSettable:inScope:onElement:");
/// Returns \c YES if the underlying audio object has the specified property in a scope and it is settable
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -propertyIsSettable:inScope:onElement:");
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope and it is settable
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

#pragma mark - Audio Object Property Retrieval

/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @return The property value
- (nullable NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -uintForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -uintForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -uintForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @return The property value
- (nullable NSArray <NSNumber *> *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -uintArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray <NSNumber *> *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -uintArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray <NSNumber *> *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -uintArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray <NSNumber *> *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a \c float or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -floatForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a \c float or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -floatForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a \c float or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -floatForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a \c float or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a \c double or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -doubleForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a \c double or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -doubleForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a \c double or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -doubleForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a \c double or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -stringForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -stringForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -stringForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -dictionaryForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -dictionaryForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -dictionaryForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioObjectForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioObjectForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -audioObjectForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioObjectArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioObjectArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -audioObjectArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a wrapped \c AudioStreamBasicDescription structure or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioStreamBasicDescription
/// @param property The property to query
/// @return The property value
- (nullable NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioStreamBasicDescriptionForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a wrapped \c AudioStreamBasicDescription structure or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioStreamBasicDescription
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioStreamBasicDescriptionForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a wrapped \c AudioStreamBasicDescription structure or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioStreamBasicDescription
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -audioStreamBasicDescriptionForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a wrapped \c AudioStreamBasicDescription structure or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioStreamBasicDescription
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of wrapped \c AudioStreamRangedDescription structures or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioStreamRangedDescription
/// @param property The property to query
/// @return The property value
- (nullable NSArray<NSValue *> *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioStreamRangedDescriptionArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of wrapped \c AudioStreamRangedDescription structures or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioStreamRangedDescription
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray<NSValue *> *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioStreamRangedDescriptionArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of wrapped \c AudioStreamRangedDescription structures or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioStreamRangedDescription
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<NSValue *> *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -audioStreamRangedDescriptionArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of wrapped \c AudioStreamRangedDescription structures or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioStreamRangedDescription
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray<NSValue *> *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a wrapped \c AudioValueRange structure or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioValueRange
/// @param property The property to query
/// @return The property value
- (nullable NSValue *)audioValueRangeForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioValueRangeForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a wrapped \c AudioValueRange structure or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSValue *)audioValueRangeForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioValueRangeForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a wrapped \c AudioValueRange structure or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSValue *)audioValueRangeForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -audioValueRangeForProperty:inScope:onElement:error:");
/// Returns the value for \c property as a wrapped \c AudioValueRange structure or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSValue *)audioValueRangeForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of wrapped \c AudioValueRange structures or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioValueRange
/// @param property The property to query
/// @return The property value
- (nullable NSArray<NSValue *> *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioValueRangeArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of wrapped \c AudioValueRange structures or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray<NSValue *> *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioValueRangeArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of wrapped \c AudioValueRange structures or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<NSValue *> *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -audioValueRangeArrayForProperty:inScope:onElement:error:");
/// Returns the value for \c property as an array of wrapped \c AudioValueRange structures or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray<NSValue *> *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Performs a block when the specified property changes
/// @note This observes \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @return \c YES on success
- (BOOL)whenPropertyChanges:(SFBAudioObjectPropertySelector)property performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:error:");
/// Performs a block when the specified property in a scope changes
/// @note This observes \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param scope The desired scope
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @return \c YES on success
- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:error:");
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @return \c YES on success
- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:error:");
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success
- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block error:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

/// AudioObject Properties
@interface SFBAudioObject (SFBAudioObjectProperties)

/// Returns the audio object's base class or \c nil on error
/// @note This corresponds to \c kAudioObjectPropertyBaseClass
@property (nonatomic, nullable, readonly) NSNumber *baseClassID NS_REFINED_FOR_SWIFT;
/// Returns the audio object's class or \c nil on error
/// @note This corresponds to \c kAudioObjectPropertyClass
@property (nonatomic, readonly) NSNumber *classID NS_REFINED_FOR_SWIFT;
/// Returns the audio object's owning object or \c nil on error
/// @note This corresponds to \c kAudioObjectPropertyOwner
@property (nonatomic, nullable, readonly) SFBAudioObject *owner NS_REFINED_FOR_SWIFT;
/// Returns the audio object's name
/// @note This corresponds to \c kAudioObjectPropertyName
/// @note The system object does not have an owner
@property (nonatomic, nullable, readonly) NSString *name NS_REFINED_FOR_SWIFT;
/// Returns the audio object's model name
/// @note This corresponds to \c kAudioObjectPropertyModelName
@property (nonatomic, nullable, readonly) NSString *modelName NS_REFINED_FOR_SWIFT;
/// Returns the audio object's manufacturer
/// @note This corresponds to \c kAudioObjectPropertyManufacturer
@property (nonatomic, nullable, readonly) NSString *manufacturer NS_REFINED_FOR_SWIFT;

/// Returns the name of the specified element in the global scope
/// @note This corresponds to \c kAudioObjectPropertyElementName
- (nullable NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -nameOfElement:inScope:");
/// Returns the name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementName
- (nullable NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_REFINED_FOR_SWIFT;

/// Returns the category name of the specified element in the global scope
/// @note This corresponds to \c kAudioObjectPropertyElementCategoryName
- (nullable NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -categoryNameOfElement:inScope:");
/// Returns the category name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementCategoryName
- (nullable NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_REFINED_FOR_SWIFT;

/// Returns the number name of the specified element in the global scope
/// @note This corresponds to \c kAudioObjectPropertyElementNumberName
- (nullable NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -numberNameOfElement:inScope:");
/// Returns the number name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementNumberName
- (nullable NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_REFINED_FOR_SWIFT;

/// Returns the audio objects owned by this object
/// @note This corresponds to \c kAudioObjectPropertyOwnedObjects
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *ownedObjects;

/// Returns the audio object's serial number
/// @note This corresponds to \c kAudioObjectPropertySerialNumber
@property (nonatomic, nullable, readonly) NSString *serialNumber;
/// Returns the audio object's firmware version
/// @note This corresponds to \c kAudioObjectPropertyFirmwareVersion
@property (nonatomic, nullable, readonly) NSString *firmwareVersion;

@end

@interface NSValue (SFBCoreAudioStructs)
/// Creates a new value object containing the specified \c AudioStreamBasicDescription structure
/// @param asbd The value for the new object
/// @return A new value object that contains \c asbd
+ (instancetype)valueWithAudioStreamBasicDescription:(AudioStreamBasicDescription)asbd;
/// Returns the \c AudioStreamBasicDescription structure representation of the value
- (AudioStreamBasicDescription)audioStreamBasicDescriptionValue;

/// Creates a new value object containing the specified \c AudioStreamRangedDescription structure
/// @param asrd The value for the new object
/// @return A new value object that contains \c asrd
+ (instancetype)valueWithAudioStreamRangedDescription:(AudioStreamRangedDescription)asrd;
/// Returns the \c AudioStreamRangedDescription structure representation of the value
- (AudioStreamRangedDescription)audioStreamRangedDescriptionValue;

/// Creates a new value object containing the specified \c AudioValueRange structure
/// @param avr The value for the new object
/// @return A new value object that contains \c avr
+ (instancetype)valueWithAudioValueRange:(AudioValueRange)avr;
/// Returns the \c AudioValueRange structure representation of the value
- (AudioValueRange)audioValueRangeValue;
@end

@interface NSNumber (SFBpid)
/// Creates a new number object containing the specified \c pid_t
/// @param pid The value for the new object
/// @return A new number object that contains \c pid
+ (instancetype)numberWithPid:(pid_t)pid;
/// Returns the \c pid_t representation of the value
- (pid_t)pidValue;
@end

NS_ASSUME_NONNULL_END
