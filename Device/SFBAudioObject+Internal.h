/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioObject.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioObjectLog;

// Audio object class determination
BOOL SFBAudioObjectIsPlugIn(AudioObjectID objectID);
BOOL SFBAudioObjectIsBox(AudioObjectID objectID);
BOOL SFBAudioObjectIsDevice(AudioObjectID objectID);
BOOL SFBAudioObjectIsClockDevice(AudioObjectID objectID);
BOOL SFBAudioObjectIsStream(AudioObjectID objectID);
BOOL SFBAudioObjectIsControl(AudioObjectID objectID);

BOOL SFBAudioPlugInIsTransportManager(AudioObjectID objectID);

BOOL SFBAudioDeviceIsAggregate(AudioObjectID objectID);
BOOL SFBAudioDeviceIsSubdevice(AudioObjectID objectID);
BOOL SFBAudioDeviceIsEndpointDevice(AudioObjectID objectID);
BOOL SFBAudioDeviceIsEndpoint(AudioObjectID objectID);

BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID);
BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID);

// Property support
BOOL SFBUInt32ForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress, UInt32 *value);
BOOL SFBFloat64ForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress, Float64 *value);

NSString * _Nullable SFBStringForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress);
NSDictionary * _Nullable SFBDictionaryForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress);

SFBAudioObject * _Nullable SFBAudioObjectForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress);
NSArray <SFBAudioObject *> * _Nullable SFBAudioObjectArrayForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress);

@interface SFBAudioObject ()
{
@protected
	/// The underlying audio object identifier
	AudioObjectID _objectID;
}
/// Returns the value for \c property as an \c UInt32 or \c 0 on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @return The property value
- (UInt32)uInt32ForProperty:(AudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c UInt32 or \c 0 on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (UInt32)uInt32ForProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope;
/// Returns the value for \c property as an \c UInt32 or \c 0 on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c UInt32
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (UInt32)uInt32ForProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Returns the value for \c property as an \c Float64 or \c 0 on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @return The property value
- (Float64)float64ForProperty:(AudioObjectPropertySelector)property;
/// Returns the value for \c property as an \c Float64 or \c 0 on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @return The property value
- (Float64)float64ForProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope;
/// Returns the value for \c property as an \c Float64 or \c 0 on error
/// @note This queries \c { property, scope, element }
/// @note \c property must refer to a property of type \c Float64
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return The property value
- (Float64)float64ForProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;
@end

NS_ASSUME_NONNULL_END
