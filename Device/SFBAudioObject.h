/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

@class SFBAudioBufferListWrapper, SFBAudioChannelLayoutWrapper;

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
	SFBAudioObjectPropertySelectorDeviceUID 							= kAudioDevicePropertyDeviceUID,
	SFBAudioObjectPropertySelectorDeviceModelUID 						= kAudioDevicePropertyModelUID,
	SFBAudioObjectPropertySelectorDeviceTransportType 					= kAudioDevicePropertyTransportType,
	SFBAudioObjectPropertySelectorDeviceRelatedDevices 					= kAudioDevicePropertyRelatedDevices,
	SFBAudioObjectPropertySelectorDeviceClockDomain 					= kAudioDevicePropertyClockDomain,
	SFBAudioObjectPropertySelectorDeviceIsAlive 						= kAudioDevicePropertyDeviceIsAlive,
	SFBAudioObjectPropertySelectorDeviceIsRunning 						= kAudioDevicePropertyDeviceIsRunning,
	SFBAudioObjectPropertySelectorDeviceCanBeDefaultDevice 				= kAudioDevicePropertyDeviceCanBeDefaultDevice,
	SFBAudioObjectPropertySelectorDeviceCanBeDefaultSystemDevice 		= kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
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
	SFBAudioObjectPropertySelectorClockDeviceUID 							= kAudioClockDevicePropertyDeviceUID,
	SFBAudioObjectPropertySelectorClockDeviceTransportType 					= kAudioClockDevicePropertyTransportType,
	SFBAudioObjectPropertySelectorClockDeviceClockDomain 					= kAudioClockDevicePropertyClockDomain,
	SFBAudioObjectPropertySelectorClockDeviceIsAlive 						= kAudioClockDevicePropertyDeviceIsAlive,
	SFBAudioObjectPropertySelectorClockDeviceIsRunning 						= kAudioClockDevicePropertyDeviceIsRunning,
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

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioObject object with the specified audio object ID
/// @note This returns a specialized subclass of \c SFBAudioObject when possible
/// @param objectID The desired audio object ID
/// @return An initialized \c SFBAudioObject object or \c nil if \c objectID is invalid or unknown
- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)objectID NS_DESIGNATED_INITIALIZER;

/// Returns the audio object's ID
@property (nonatomic, readonly) AudioObjectID objectID;

@end

@interface SFBAudioObject (SFBPropertyBasics)

#pragma mark - Property Information

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

/// Returns \c @ YES if the underlying audio object has the specified property and it is settable or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
- (nullable NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -propertyIsSettable:inScope:onElement:error:");
/// Returns \c @ YES if the underlying audio object has the specified property in a scope and it is settable or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
- (nullable NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -propertyIsSettable:inScope:onElement:error:");
/// Returns \c @ YES if the underlying audio object has the specified property on an element in a scope and it is settable or \c nil on error
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
- (nullable NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_UNAVAILABLE("Use -propertyIsSettable:inScope:onElement:error:");
/// Returns \c @ YES if the underlying audio object has the specified property on an element in a scope and it is settable or \c nil on error
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
- (nullable NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

#pragma mark - Property Observation

/// Performs a block when the specified property changes
/// @note This observes \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @return \c YES if the property listener was successfully added, \c NO otherwise
- (BOOL)whenPropertyChanges:(SFBAudioObjectPropertySelector)property performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:error:");
/// Performs a block when the specified property in a scope changes
/// @note This observes \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param scope The desired scope
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @return \c YES if the property listener was successfully added, \c NO otherwise
- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:error:");
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @return \c YES if the property listener was successfully added, \c NO otherwise
- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:error:");
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property listener was successfully added, \c NO otherwise
- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block error:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

/// Property Retrieval
@interface SFBAudioObject (SFBPropertyGetters)

#pragma mark - Property Retrieval

/// Returns the value for \c property as a \c unsigned \c int or \c nil on error
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSNumber *)unsignedIntForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of \c unsigned \c int or \c nil on error
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray <NSNumber *> *)unsignedIntArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a \c float or \c nil on error
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a \c double or \c nil on error
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_SWIFT_UNAVAILABLE("Use -stringForProperty:inScope:onElement:qualifier:qualifierSize:error:");
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param qualifier An optonal pointer to a property qualifier
/// @param qualifierSize The size, in bytes, of the data pointed to by \c qualifier
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element qualifier:(nullable const void *)qualifier qualifierSize:(UInt32)qualifierSize error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of \c CFTypeRef objects or \c nil on error
/// @note \c property must refer to a property of type array of \c CFArrayRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray *)arrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c NSURL object or \c nil on error
/// @note \c property must refer to a property of type \c CFURLRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSURL *)urlForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note \c property must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_SWIFT_UNAVAILABLE("Use -audioObjectForProperty:inScope:onElement:qualifier:qualifierSize:error:");
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note \c property must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param qualifier An optonal pointer to a property qualifier
/// @param qualifierSize The size, in bytes, of the data pointed to by \c qualifier
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element qualifier:(nullable const void *)qualifier qualifierSize:(UInt32)qualifierSize error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note \c property must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_SWIFT_UNAVAILABLE("Use -audioObjectArrayForProperty:inScope:onElement:qualifier:qualifierSize:error:");
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note \c property must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param qualifier An optonal pointer to a property qualifier
/// @param qualifierSize The size, in bytes, of the data pointed to by \c qualifier
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element qualifier:(nullable const void *)qualifier qualifierSize:(UInt32)qualifierSize error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a wrapped \c AudioStreamBasicDescription structure or \c nil on error
/// @note \c property must refer to a property of type \c AudioStreamBasicDescription
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of wrapped \c AudioStreamRangedDescription structures or \c nil on error
/// @note \c property must refer to a property of type array of \c AudioStreamRangedDescription
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray<NSValue *> *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a wrapped \c AudioValueRange structure or \c nil on error
/// @note \c property must refer to a property of type \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSValue *)audioValueRangeForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of wrapped \c AudioValueRange structures or \c nil on error
/// @note \c property must refer to a property of type array of \c AudioValueRange
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable NSArray<NSValue *> *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a wrapped \c AudioChannelLayout structure or \c nil on error
/// @note \c property must refer to a property of type \c AudioChannelLayout
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable SFBAudioChannelLayoutWrapper *)audioChannelLayoutForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a wrapped \c AudioBufferList structure or \c nil on error
/// @note \c property must refer to a property of type \c AudioBufferList
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable SFBAudioBufferListWrapper *)audioBufferListForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c os_workgroup_t object or \c nil on error
/// @note \c property must refer to a property of type \c os_workgroup_t
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The property value
- (nullable os_workgroup_t)osWorkgroupForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error API_AVAILABLE(macos(11.0)) NS_REFINED_FOR_SWIFT;

@end

/// Property Translation
@interface SFBAudioObject (SFBPropertyTranslation)

/// Translates \c value using an \c AudioValueTranslation structure and returns the translated value or \c nil on error
/// @note \c property must accept an \c AudioValueTranslation structure having \c UInt32 for input and \c CFStringRef for output
/// @param value The value to translate
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The translated value
- (nullable NSString *)translateToStringFromUnsignedInteger:(UInt32)value usingProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Translates \c value using an \c AudioValueTranslation structure and returns the translated value or \c nil on error
/// @note \c property must accept an \c AudioValueTranslation structure having \c UInt32 for input and \c UInt32 for output
/// @param value The value to translate
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return The translated value
- (nullable NSNumber *)translateToUnsignedIntegerFromUnsignedInteger:(UInt32)value usingProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

/// Property Setting
@interface SFBAudioObject (SFBPropertySetters)

#pragma mark - Property Setting

/// Sets the value for \c property as an \c unsigned \c int
/// @note \c property must refer to a property of type \c UInt32
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setUnsignedInt:(UInt32)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Sets the value for \c property as an array of \c unsigned \c int
/// @note \c property must refer to a property of type array of \c UInt32
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setUnsignedIntArray:(NSArray<NSNumber *> *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Sets the value for \c property as a \c float
/// @note \c property must refer to a property of type \c Float32
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setFloat:(float)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Sets the value for \c property as a \c double
/// @note \c property must refer to a property of type \c Float64
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setDouble:(double)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Sets the value for \c property as an \c AudioStreamBasicDescription
/// @note \c property must refer to a property of type \c AudioStreamBasicDescription
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setAudioStreamBasicDescription:(AudioStreamBasicDescription)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Sets the value for \c property as an \c SFBAudioObject object
/// @note \c property must refer to a property of type \c AudioObjectID
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setAudioObject:(SFBAudioObject *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Sets the value for \c property as an \c SFBAudioChannelLayoutWrapper
/// @note \c property must refer to a property of type \c AudioChannelLayout
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setAudioChannelLayout:(SFBAudioChannelLayoutWrapper *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Sets the value for \c property as an \c SFBAudioBufferListWrapper
/// @note \c property must refer to a property of type \c AudioBufferList
/// @param value The desired value
/// @param property The property to set
/// @param scope The desired scope
/// @param element The desired element
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if the property was set successfully, \c NO otherwise
- (BOOL)setAudioBufferList:(SFBAudioBufferListWrapper *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

/// AudioObject Properties
@interface SFBAudioObject (SFBAudioObjectProperties)

#pragma mark - AudioObject Properties

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

/// Returns the name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementName
- (nullable NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_REFINED_FOR_SWIFT;
/// Returns the category name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementCategoryName
- (nullable NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_REFINED_FOR_SWIFT;
/// Returns the number name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementNumberName
- (nullable NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_REFINED_FOR_SWIFT;

/// Returns the audio objects owned by this object
/// @note This corresponds to \c kAudioObjectPropertyOwnedObjects
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *ownedObjects;
/// Returns the audio objects of the specified types owned by this object
/// @note This corresponds to \c kAudioObjectPropertyOwnedObjects
/// @param types An array of wrapped \c AudioClassIDs
- (nullable NSArray<SFBAudioObject *> *)ownedObjectsOfType:(NSArray<NSNumber *> *)types NS_REFINED_FOR_SWIFT;

/// Returns the audio object's serial number
/// @note This corresponds to \c kAudioObjectPropertySerialNumber
@property (nonatomic, nullable, readonly) NSString *serialNumber;
/// Returns the audio object's firmware version
/// @note This corresponds to \c kAudioObjectPropertyFirmwareVersion
@property (nonatomic, nullable, readonly) NSString *firmwareVersion;

@end

#pragma mark - NSValue extension for fixed-length Core Audio structures

/// \c NSValue support for \c CoreAudio structures
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

#pragma mark - NSNumber extension for pid_t

/// \c NSNumber support for \c pid_t
@interface NSNumber (SFBpid)
/// Creates a new number object containing the specified \c pid_t
/// @param pid The value for the new object
/// @return A new number object that contains \c pid
+ (instancetype)numberWithPid:(pid_t)pid;
/// Returns the \c pid_t representation of the value
- (pid_t)pidValue;
@end

#pragma mark - Wrappers for variable-length Core Audio structures

/// A thin wrapper around a variable-length \c AudioBufferList structure
NS_SWIFT_NAME(AudioBufferListWrapper) @interface SFBAudioBufferListWrapper : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;


/// Returns an initialized \c SFBAudioBufferListWrapper object wrapping the specified \c AudioBufferList
/// @note If \c freeWhenDone is \c YES \c audioBufferList must have been allocated using \c malloc
/// @note If \c freeWhenDone is \c YES the object takes ownership of \c audioBufferList
/// @param audioBufferList The \c AudioBufferList structure to wrap
/// @param freeWhenDone Whether the memory for \c audioBufferList should be reclaimed using \c free
/// @return An initialized \c SFBAudioBufferListWrapper object
- (instancetype)initWithAudioBufferList:(AudioBufferList *)audioBufferList freeWhenDone:(BOOL)freeWhenDone NS_DESIGNATED_INITIALIZER;

/// Returns an initialized \c SFBAudioBufferListWrapper object with the specified number of channel descriptions
/// @param numberBuffers The number of buffers in the buffer list
/// @return An initialized \c SFBAudioBufferListWrapper object
- (instancetype)initWithNumberBuffers:(UInt32)numberBuffers;

/// Returns the underlying \c AudioBufferList structure
@property (nonatomic, readonly) const AudioBufferList *audioBufferList;

/// Returns the buffer list's \c mNumberBuffers
@property (nonatomic, readonly) UInt32 numberBuffers NS_SWIFT_UNAVAILABLE("Use -buffers");
/// Returns the buffer list's \c mBuffers or \c NULL if \c mNumberBuffers is zero
@property (nonatomic, nullable, readonly) const AudioBuffer *buffers NS_REFINED_FOR_SWIFT;

@end

/// A thin wrapper around a variable-length \c AudioChannelLayout structure
NS_SWIFT_NAME(AudioChannelLayoutWrapper) @interface SFBAudioChannelLayoutWrapper : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioChannelLayoutWrapper object wrapping the specified \c AudioChannelLayout
/// @note If \c freeWhenDone is \c YES \c audioChannelLayout must have been allocated using \c malloc
/// @note If \c freeWhenDone is \c YES the object takes ownership of \c audioChannelLayout
/// @param audioChannelLayout The \c AudioChannelLayout structure to wrap
/// @param freeWhenDone Whether the memory for \c audioChannelLayout should be reclaimed using \c free
/// @return An initialized \c SFBAudioChannelLayoutWrapper object
- (instancetype)initWithAudioChannelLayout:(AudioChannelLayout *)audioChannelLayout freeWhenDone:(BOOL)freeWhenDone NS_DESIGNATED_INITIALIZER;

/// Returns an initialized \c SFBAudioChannelLayoutWrapper object with a copy of the specified \c AudioChannelLayout
/// @note A copy of \c audioChannelLayout is made
/// @param audioChannelLayout The \c AudioChannelLayout structure to wrap
/// @return An initialized \c SFBAudioChannelLayoutWrapper object
- (instancetype)initWithAudioChannelLayout:(AudioChannelLayout *)audioChannelLayout;

/// Returns an initialized \c SFBAudioChannelLayoutWrapper object with the specified number of channel descriptions
/// @param numberChannelDescriptions The number of channel descriptions in the layout
/// @return An initialized \c SFBAudioChannelLayoutWrapper object
- (nullable instancetype)initWithNumberChannelDescriptions:(UInt32)numberChannelDescriptions;

/// Returns the underlying \c AudioChannelLayout structure
@property (nonatomic, readonly) const AudioChannelLayout *audioChannelLayout;

/// Returns the layout's \c mAudioChannelLayoutTag
@property (nonatomic, readonly) AudioChannelLayoutTag tag;
/// Returns the layout's \c mAudioChannelBitmap
@property (nonatomic, readonly) AudioChannelBitmap bitmap;

/// Returns the layout's \c mNumberChannelDescriptions
@property (nonatomic, readonly) UInt32 numberChannelDescriptions NS_SWIFT_UNAVAILABLE("Use -channelDescriptions");
/// Returns the layout's \c mChannelDescriptions or \c NULL if \c mNumberChannelDescriptions is zero
@property (nonatomic, nullable, readonly) const AudioChannelDescription *channelDescriptions NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
