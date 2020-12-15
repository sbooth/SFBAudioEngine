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
	// AudioObject properties
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
	SFBAudioObjectPropertySelectorSerialNumber 			= kAudioObjectPropertySerialNumber,
	SFBAudioObjectPropertySelectorFirmwareVersion 		= kAudioObjectPropertyFirmwareVersion,

	// AudioPlugIn properties
	SFBAudioObjectPropertySelectorPlugInBundleID 			= kAudioPlugInPropertyBundleID,
	SFBAudioObjectPropertySelectorPlugInDeviceList 			= kAudioPlugInPropertyDeviceList,
	SFBAudioObjectPropertySelectorPlugInBoxList 			= kAudioPlugInPropertyBoxList,
	SFBAudioObjectPropertySelectorPlugInClockDeviceList 	= kAudioPlugInPropertyClockDeviceList,

	// AudioTransportManager properties
	SFBAudioObjectPropertySelectorTransportManagerEndPointList 		= kAudioTransportManagerPropertyEndPointList,
	SFBAudioObjectPropertySelectorTransportManagerTransportType 	= kAudioTransportManagerPropertyTransportType,

	// AudioBox properties
	SFBAudioObjectPropertySelectorBoxUID 				= kAudioBoxPropertyBoxUID,
	SFBAudioObjectPropertySelectorBoxTransportType 		= kAudioBoxPropertyTransportType,
	SFBAudioObjectPropertySelectorBoxHasAudio 			= kAudioBoxPropertyHasAudio,
	SFBAudioObjectPropertySelectorBoxHasVideo 			= kAudioBoxPropertyHasVideo,
	SFBAudioObjectPropertySelectorBoxHasMIDI 			= kAudioBoxPropertyHasMIDI,
	SFBAudioObjectPropertySelectorBoxAcquired 			= kAudioBoxPropertyAcquired,
	SFBAudioObjectPropertySelectorBoxDeviceList 		= kAudioBoxPropertyDeviceList,
	SFBAudioObjectPropertySelectorBoxClockDeviceList 	= kAudioBoxPropertyClockDeviceList,

	// AudioDevice properties
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

	// AudioClockDevice properties
	SFBAudioObjectPropertySelectorClockDeviceDeviceUID 						= kAudioClockDevicePropertyDeviceUID,
	SFBAudioObjectPropertySelectorClockDeviceTransportType 					= kAudioClockDevicePropertyTransportType,
	SFBAudioObjectPropertySelectorClockDeviceClockDomain 					= kAudioClockDevicePropertyClockDomain,
	SFBAudioObjectPropertySelectorClockDeviceDeviceIsAlive 					= kAudioClockDevicePropertyDeviceIsAlive,
	SFBAudioObjectPropertySelectorClockDeviceDeviceIsRunning 				= kAudioClockDevicePropertyDeviceIsRunning,
	SFBAudioObjectPropertySelectorClockDeviceLatency 						= kAudioClockDevicePropertyLatency,
	SFBAudioObjectPropertySelectorClockDeviceControlList 					= kAudioClockDevicePropertyControlList,
	SFBAudioObjectPropertySelectorClockDeviceNominalSampleRate 				= kAudioClockDevicePropertyNominalSampleRate,
	SFBAudioObjectPropertySelectorClockDeviceAvailableNominalSampleRates 	= kAudioClockDevicePropertyAvailableNominalSampleRates,

	// AudioEndPointDevice properties
	SFBAudioObjectPropertySelectorEndpointDeviceComposition 	= kAudioEndPointDevicePropertyComposition,
	SFBAudioObjectPropertySelectorEndpointDeviceEndPointList 	= kAudioEndPointDevicePropertyEndPointList,
	SFBAudioObjectPropertySelectorEndpointDeviceIsPrivate 		= kAudioEndPointDevicePropertyIsPrivate,

	// AudioStream properties
	SFBAudioObjectPropertySelectorStreamIsActive 					= kAudioStreamPropertyIsActive,
	SFBAudioObjectPropertySelectorStreamDirection 					= kAudioStreamPropertyDirection,
	SFBAudioObjectPropertySelectorStreamTerminalType 				= kAudioStreamPropertyTerminalType,
	SFBAudioObjectPropertySelectorStreamStartingChannel 			= kAudioStreamPropertyStartingChannel,
	SFBAudioObjectPropertySelectorStreamLatency 					= kAudioStreamPropertyLatency,
	SFBAudioObjectPropertySelectorStreamVirtualFormat 				= kAudioStreamPropertyVirtualFormat,
	SFBAudioObjectPropertySelectorStreamAvailableVirtualFormats 	= kAudioStreamPropertyAvailableVirtualFormats,
	SFBAudioObjectPropertySelectorStreamPhysicalFormat 				= kAudioStreamPropertyPhysicalFormat,
	SFBAudioObjectPropertySelectorStreamAvailablePhysicalFormats 	= kAudioStreamPropertyAvailablePhysicalFormats,

	/// Wildcard  selector, useful for notifications
	SFBAudioObjectPropertySelectorWildcard 		= kAudioObjectPropertySelectorWildcard
} NS_SWIFT_NAME(AudioObject.PropertySelector);

/// Property scopes for \c SFBAudioObject and subclasses
/// @note These are interchangeable with \c AudioObjectPropertyScope but are typed for ease of use from Swift.
typedef NS_ENUM(AudioObjectPropertyScope, SFBAudioObjectPropertyScope) {
	/// Global scope
	SFBAudioObjectPropertyScopeGlobal			= kAudioObjectPropertyScopeGlobal,
	/// Input scope
	SFBAudioObjectPropertyScopeInput			= kAudioObjectPropertyScopeInput,
	/// Output scope
	SFBAudioObjectPropertyScopeOutput 			= kAudioObjectPropertyScopeOutput,
	/// Playthrough scope
	SFBAudioObjectPropertyScopePlayThrough 		= kAudioObjectPropertyScopePlayThrough,
	/// Wildcard  scope, useful for notifications
	SFBAudioObjectPropertyScopeWildcard			= kAudioObjectPropertyScopeWildcard
} NS_SWIFT_NAME(AudioObject.PropertyScope);

/// Audio device transport types
typedef NS_ENUM(UInt32, SFBAudioDeviceTransportType) {
	/// Unknown
	SFBAudioDeviceTransportTypeUnknown 			= kAudioDeviceTransportTypeUnknown,
	/// Built-in
	SFBAudioDeviceTransportTypeBuiltIn 			= kAudioDeviceTransportTypeBuiltIn,
	/// Aggregate device
	SFBAudioDeviceTransportTypeAggregate 		= kAudioDeviceTransportTypeAggregate,
	/// Virtual device
	SFBAudioDeviceTransportTypeVirtual 			= kAudioDeviceTransportTypeVirtual,
	/// PCI
	SFBAudioDeviceTransportTypePCI 				= kAudioDeviceTransportTypePCI,
	/// USB
	SFBAudioDeviceTransportTypeUSB 				= kAudioDeviceTransportTypeUSB,
	/// FireWire
	SFBAudioDeviceTransportTypeFireWire 		= kAudioDeviceTransportTypeFireWire,
	/// Bluetooth
	SFBAudioDeviceTransportTypeBluetooth 		= kAudioDeviceTransportTypeBluetooth,
	/// Bluetooth Low Energy
	SFBAudioDeviceTransportTypeBluetoothLE 		= kAudioDeviceTransportTypeBluetoothLE,
	/// HDMI
	SFBAudioDeviceTransportTypeHDMI 			= kAudioDeviceTransportTypeHDMI,
	/// DisplayPort
	SFBAudioDeviceTransportTypeDisplayPort 		= kAudioDeviceTransportTypeDisplayPort,
	/// AirPlay
	SFBAudioDeviceTransportTypeAirPlay 			= kAudioDeviceTransportTypeAirPlay,
	/// AVB
	SFBAudioDeviceTransportTypeAVB 				= kAudioDeviceTransportTypeAVB,
	/// Thunderbolt
	SFBAudioDeviceTransportTypeThunderbolt 		= kAudioDeviceTransportTypeThunderbolt
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
- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property;
/// Returns \c YES if the underlying audio object has the specified property in a scope
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(hasProperty(_:scope:));
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(hasProperty(_:scope:element:));

/// Returns \c YES if the underlying audio object has the specified property and it is settable
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property;
/// Returns \c YES if the underlying audio object has the specified property in a scope and it is settable
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(propertyIsSettable(_:scope:));
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope and it is settable
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(propertyIsSettable(_:scope:element:));

/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(string(forProperty:scope:));
/// Returns the value for \c property as an \c NSString object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFStringRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(string(forProperty:scope:element:));

/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(dictionary(forProperty:scope:));
/// Returns the value for \c property as an \c NSDictionary object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c CFDictionaryRef
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(dictionary(forProperty:scope:element:));

/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(audioObject(forProperty:scope:));
/// Returns the value for \c property as an \c SFBAudioObject object or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(audioObject(forProperty:scope:element:));

/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectsForProperty:(SFBAudioObjectPropertySelector)property;
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectsForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope NS_SWIFT_NAME(audioObjects(forProperty:scope:));
/// Returns the value for \c property as an array of \c SFBAudioObject objects or \c nil on error
/// @note This queries \c { property, scope, element }
/// @note \c propertyAddress must refer to a property of type array of \c AudioObjectID
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (nullable NSArray<SFBAudioObject *> *)audioObjectsForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element NS_SWIFT_NAME(audioObjects(forProperty:scope:element:));

/// Performs a block when the specified property changes
/// @note This observes \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenPropertyChanges:(SFBAudioObjectPropertySelector)property performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the specified property in a scope changes
/// @note This observes \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param scope The desired scope
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(SFBAudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenPropertyChanges(_:scope:perform:));
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block NS_SWIFT_NAME(whenPropertyChanges(_:scope:element:perform:));

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
