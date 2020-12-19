/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioPlugIn.h"
#import "SFBAudioObject+Internal.h"

#import "SFBAggregateDevice.h"
#import "SFBCStringForOSType.h"

@implementation SFBAudioPlugIn

+ (NSArray *)plugIns
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyPlugInList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioPlugInClassID));
	return [super initWithAudioObjectID:objectID];
}

- (instancetype)initWithBundleID:(NSString *)bundleID
{
	NSParameterAssert(bundleID != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyTranslateBundleIDToPlugIn,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID objectID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(objectID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(bundleID), &bundleID, &specifierSize, &objectID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyTranslateBundleIDToPlugIn) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(objectID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioObjectLog, "Unknown audio plugin bundle ID: %{public}@", bundleID);
		return nil;
	}

	return [self initWithAudioObjectID:objectID];
}

- (SFBAggregateDevice *)createAggregateDevice:(NSDictionary *)composition error:(NSError **)error
{
	NSParameterAssert(composition != nil);
	CFDictionaryRef qualifier = (__bridge CFDictionaryRef)composition;
	return (SFBAggregateDevice *)[self audioObjectForProperty:kAudioPlugInCreateAggregateDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:qualifier qualifierSize:sizeof(qualifier) error:error];
}

- (BOOL)destroyAggregateDevice:(SFBAggregateDevice *)aggregateDevice error:(NSError **)error
{
	NSParameterAssert(aggregateDevice != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioPlugInDestroyAggregateDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID value = aggregateDevice.objectID;
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioPlugInDestroyAggregateDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (NSString *)bundleID
{
	return [self stringForProperty:kAudioPlugInPropertyBundleID inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)devices
{
	return [self audioObjectArrayForProperty:kAudioPlugInPropertyDeviceList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioDevice *)deviceForUID:(NSString *)deviceUID
{
	NSParameterAssert(deviceUID != nil);
	CFStringRef qualifier = (__bridge CFStringRef)deviceUID;
	return (SFBAudioDevice *)[self audioObjectForProperty:kAudioPlugInPropertyTranslateUIDToDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:qualifier qualifierSize:sizeof(qualifier) error:NULL];
}

- (NSArray *)boxes
{
	return [self audioObjectArrayForProperty:kAudioPlugInPropertyBoxList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioBox *)boxForUID:(NSString *)boxUID
{
	NSParameterAssert(boxUID != nil);
	CFStringRef qualifier = (__bridge CFStringRef)boxUID;
	return (SFBAudioBox *)[self audioObjectForProperty:kAudioPlugInPropertyTranslateUIDToBox inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:qualifier qualifierSize:sizeof(qualifier) error:NULL];
}

- (NSArray *)clockDevices
{
	return [self audioObjectArrayForProperty:kAudioPlugInPropertyClockDeviceList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBClockDevice *)clockDeviceForUID:(NSString *)clockDeviceUID
{
	NSParameterAssert(clockDeviceUID != nil);
	CFStringRef qualifier = (__bridge CFStringRef)clockDeviceUID;
	return (SFBClockDevice *)[self audioObjectForProperty:kAudioPlugInPropertyTranslateUIDToClockDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:qualifier qualifierSize:sizeof(qualifier) error:NULL];
}

@end
