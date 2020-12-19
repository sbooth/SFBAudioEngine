/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioPlugIn.h"
#import "SFBAudioObject+Internal.h"

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
