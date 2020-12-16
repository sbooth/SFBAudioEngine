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
	SFBAudioObjectPropertySelectorPanControlValue 				= kAudioStereoPanControlPropertyValue,
	SFBAudioObjectPropertySelectorPanControlPanningChannels 	= kAudioStereoPanControlPropertyPanningChannels,

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

	SFBAudioObjectPropertySelectorDeviceJackIsConnected = kAudioDevicePropertyJackIsConnected,
	SFBAudioObjectPropertySelectorDeviceVolumeScalar = kAudioDevicePropertyVolumeScalar,
	SFBAudioObjectPropertySelectorDeviceVolumeDecibels = kAudioDevicePropertyVolumeDecibels,
	SFBAudioObjectPropertySelectorDeviceVolumeRangeDecibels = kAudioDevicePropertyVolumeRangeDecibels,
	SFBAudioObjectPropertySelectorDeviceVolumeScalarToDecibels = kAudioDevicePropertyVolumeScalarToDecibels,
	SFBAudioObjectPropertySelectorDeviceVolumeDecibelsToScalar = kAudioDevicePropertyVolumeDecibelsToScalar,
	SFBAudioObjectPropertySelectorDeviceStereoPan = kAudioDevicePropertyStereoPan,
	SFBAudioObjectPropertySelectorDeviceStereoPanChannels = kAudioDevicePropertyStereoPanChannels,
	SFBAudioObjectPropertySelectorDeviceMute = kAudioDevicePropertyMute,
	SFBAudioObjectPropertySelectorDeviceSolo = kAudioDevicePropertySolo,
	SFBAudioObjectPropertySelectorDevicePhantomPower = kAudioDevicePropertyPhantomPower,
	SFBAudioObjectPropertySelectorDevicePhaseInvert = kAudioDevicePropertyPhaseInvert,
	SFBAudioObjectPropertySelectorDeviceClipLight = kAudioDevicePropertyClipLight,
	SFBAudioObjectPropertySelectorDeviceTalkback = kAudioDevicePropertyTalkback,
	SFBAudioObjectPropertySelectorDeviceListenback = kAudioDevicePropertyListenback,
	SFBAudioObjectPropertySelectorDeviceDataSource = kAudioDevicePropertyDataSource,
	SFBAudioObjectPropertySelectorDeviceDataSources = kAudioDevicePropertyDataSources,
	SFBAudioObjectPropertySelectorDeviceDataSourceNameForIDCFString = kAudioDevicePropertyDataSourceNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceDataSourceKindForID = kAudioDevicePropertyDataSourceKindForID,
	SFBAudioObjectPropertySelectorDeviceClockSource = kAudioDevicePropertyClockSource,
	SFBAudioObjectPropertySelectorDeviceClockSources = kAudioDevicePropertyClockSources,
	SFBAudioObjectPropertySelectorDeviceClockSourceNameForIDCFString = kAudioDevicePropertyClockSourceNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceClockSourceKindForID = kAudioDevicePropertyClockSourceKindForID,
	SFBAudioObjectPropertySelectorDevicePlayThru = kAudioDevicePropertyPlayThru,
	SFBAudioObjectPropertySelectorDevicePlayThruSolo = kAudioDevicePropertyPlayThruSolo,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeScalar = kAudioDevicePropertyPlayThruVolumeScalar,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeDecibels = kAudioDevicePropertyPlayThruVolumeDecibels,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeRangeDecibels = kAudioDevicePropertyPlayThruVolumeRangeDecibels,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeScalarToDecibels = kAudioDevicePropertyPlayThruVolumeScalarToDecibels,
	SFBAudioObjectPropertySelectorDevicePlayThruVolumeDecibelsToScalar = kAudioDevicePropertyPlayThruVolumeDecibelsToScalar,
	SFBAudioObjectPropertySelectorDevicePlayThruStereoPan = kAudioDevicePropertyPlayThruStereoPan,
	SFBAudioObjectPropertySelectorDevicePlayThruStereoPanChannels = kAudioDevicePropertyPlayThruStereoPanChannels,
	SFBAudioObjectPropertySelectorDevicePlayThruDestination = kAudioDevicePropertyPlayThruDestination,
	SFBAudioObjectPropertySelectorDevicePlayThruDestinations = kAudioDevicePropertyPlayThruDestinations,
	SFBAudioObjectPropertySelectorDevicePlayThruDestinationNameForIDCFString = kAudioDevicePropertyPlayThruDestinationNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceChannelNominalLineLevel = kAudioDevicePropertyChannelNominalLineLevel,
	SFBAudioObjectPropertySelectorDeviceChannelNominalLineLevels = kAudioDevicePropertyChannelNominalLineLevels,
	SFBAudioObjectPropertySelectorDeviceChannelNominalLineLevelNameForIDCFString = kAudioDevicePropertyChannelNominalLineLevelNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceHighPassFilterSetting = kAudioDevicePropertyHighPassFilterSetting,
	SFBAudioObjectPropertySelectorDeviceHighPassFilterSettings = kAudioDevicePropertyHighPassFilterSettings,
	SFBAudioObjectPropertySelectorDeviceHighPassFilterSettingNameForIDCFString = kAudioDevicePropertyHighPassFilterSettingNameForIDCFString,
	SFBAudioObjectPropertySelectorDeviceSubVolumeScalar = kAudioDevicePropertySubVolumeScalar,
	SFBAudioObjectPropertySelectorDeviceSubVolumeDecibels = kAudioDevicePropertySubVolumeDecibels,
	SFBAudioObjectPropertySelectorDeviceSubVolumeRangeDecibels = kAudioDevicePropertySubVolumeRangeDecibels,
	SFBAudioObjectPropertySelectorDeviceSubVolumeScalarToDecibels = kAudioDevicePropertySubVolumeScalarToDecibels,
	SFBAudioObjectPropertySelectorDeviceSubVolumeDecibelsToScalar = kAudioDevicePropertySubVolumeDecibelsToScalar,
	SFBAudioObjectPropertySelectorDeviceSubMute = kAudioDevicePropertySubMute,

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

#pragma mark - Audio Object Properties

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

/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @return The property value
- (nullable NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -uintForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -uintForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @return The property value
- (nullable NSArray <NSNumber *> *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -uintArrayForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray <NSNumber *> *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -uintArrayForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c UInt32 or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type array of \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray <NSNumber *> *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a \c float or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -floatForProperty:inScope:onElement:");
/// Returns the value for \c property as a \c float or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -floatForProperty:inScope:onElement:");
/// Returns the value for \c property as a \c float or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as a \c double or \c nil on error
/// @note This queries \c { property, SFBAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -doubleForProperty:inScope:onElement:");
/// Returns the value for \c property as a \c double or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -doubleForProperty:inScope:onElement:");
/// Returns the value for \c property as a \c double or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -stringForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -stringForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -dictionaryForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -dictionaryForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioObjectForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioObjectForProperty:inScope:onElement:");
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property NS_SWIFT_UNAVAILABLE("Use -audioObjectArrayForProperty:inScope:onElement:");
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_UNAVAILABLE("Use -audioObjectArrayForProperty:inScope:onElement:");
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_REFINED_FOR_SWIFT;

/// Performs a block when the specified property changes
/// @note This observes \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenPropertyChanges:(SFBAudioObjectPropertySelector)property performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:");
/// Performs a block when the specified property in a scope changes
/// @note This observes \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param scope The desired scope
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(SFBAudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_UNAVAILABLE("Use -whenPropertyChanges:inScope:onElement:performBlock:");
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block NS_REFINED_FOR_SWIFT;

@end

/// AudioObject Properties
@interface SFBAudioObject (SFBAudioObjectProperties)

/// Returns the audio object's base class or \c 0 on error
/// @note This corresponds to \c kAudioObjectPropertyBaseClass
@property (nonatomic, readonly) AudioClassID baseClassID;
/// Returns the audio object's class or \c 0 on error
/// @note This corresponds to \c kAudioObjectPropertyClass
@property (nonatomic, readonly) AudioClassID classID;
/// Returns the audio object's owning object
/// @note This corresponds to \c kAudioObjectPropertyOwner
@property (nonatomic, readonly) SFBAudioObject *owner;
/// Returns the audio object's name
/// @note This corresponds to \c kAudioObjectPropertyName
@property (nonatomic, nullable, readonly) NSString *name;
/// Returns the audio object's model name
/// @note This corresponds to \c kAudioObjectPropertyModelName
@property (nonatomic, nullable, readonly) NSString *modelName;
/// Returns the audio object's manufacturer
/// @note This corresponds to \c kAudioObjectPropertyManufacturer
@property (nonatomic, nullable, readonly) NSString *manufacturer;

/// Returns the name of the specified element in the global scope
/// @note This corresponds to \c kAudioObjectPropertyElementName
- (NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(nameOfElement(_:));
/// Returns the name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementName
- (NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(nameOfElement(_:scope:));

/// Returns the category name of the specified element in the global scope
/// @note This corresponds to \c kAudioObjectPropertyElementCategoryName
- (NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(categoryNameOfElement(_:));
/// Returns the category name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementCategoryName
- (NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(categoryNameOfElement(_:scope:));

/// Returns the number name of the specified element in the global scope
/// @note This corresponds to \c kAudioObjectPropertyElementNumberName
- (NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(numberNameOfElement(_:));
/// Returns the number name of the specified element in the specified scope
/// @note This corresponds to \c kAudioObjectPropertyElementNumberName
- (NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(numberNameOfElement(_:scope:));

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

NS_ASSUME_NONNULL_END
