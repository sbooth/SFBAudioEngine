/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAggregateAudioDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBAudioClockDevice.h"
#import "SFBCStringForOSType.h"

@implementation SFBAggregateAudioDevice

+ (NSArray *)aggregateDevices
{
	NSMutableArray *aggregateDevices = [NSMutableArray array];

	NSArray *devices = [SFBAudioDevice devices];
	for(SFBAudioDevice *device in devices) {
		if(device.isAggregate)
			[aggregateDevices addObject:device];
	}

	return aggregateDevices;
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID
{
	NSParameterAssert(audioObjectID != kAudioObjectUnknown);
	NSParameterAssert(SFBAudioDeviceIsAggregate(audioObjectID));

	return [super initWithAudioObjectID:audioObjectID];
}

- (NSArray *)allSubdevices
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioAggregateDevicePropertyFullSubDeviceList,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFArrayRef fullSubDeviceList = NULL;
	UInt32 dataSize = sizeof(fullSubDeviceList);
	OSStatus result = AudioObjectGetPropertyData(self.deviceID, &propertyAddress, 0, NULL, &dataSize, &fullSubDeviceList);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioAggregateDevicePropertyFullSubDeviceList) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSArray *)fullSubDeviceList;
}

- (NSArray *)activeSubdevices
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioAggregateDevicePropertyActiveSubDeviceList,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(self.deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioAggregateDevicePropertyActiveSubDeviceList) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioObjectID *deviceIDs = (AudioObjectID *)malloc(dataSize);
	if(!deviceIDs) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(self.deviceID, &propertyAddress, 0, NULL, &dataSize, deviceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioAggregateDevicePropertyActiveSubDeviceList) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(deviceIDs);
		return nil;
	}

	NSMutableArray *subdevices = [NSMutableArray array];
	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioObjectID)); ++i) {
		SFBAudioDevice *device = [[SFBAudioDevice alloc] initWithAudioObjectID:deviceIDs[i]];
		if(device)
			[subdevices addObject:device];
	}

	free(deviceIDs);

	return subdevices;
}

- (NSDictionary *)composition
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioAggregateDevicePropertyComposition,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFDictionaryRef composition = NULL;
	UInt32 dataSize = sizeof(composition);
	OSStatus result = AudioObjectGetPropertyData(self.deviceID, &propertyAddress, 0, NULL, &dataSize, &composition);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioAggregateDevicePropertyComposition) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSDictionary *)composition;
}

- (SFBAudioDevice *)masterSubdevice
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioAggregateDevicePropertyMasterSubDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef masterSubDevice = NULL;
	UInt32 dataSize = sizeof(masterSubDevice);
	OSStatus result = AudioObjectGetPropertyData(self.deviceID, &propertyAddress, 0, NULL, &dataSize, &masterSubDevice);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioAggregateDevicePropertyMasterSubDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return [[SFBAudioDevice alloc] initWithDeviceUID:(__bridge_transfer NSString *)masterSubDevice];
}

- (SFBAudioClockDevice *)clockDevice
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioAggregateDevicePropertyClockDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef clockDeviceUID = NULL;
	UInt32 dataSize = sizeof(clockDeviceUID);
	OSStatus result = AudioObjectGetPropertyData(self.deviceID, &propertyAddress, 0, NULL, &dataSize, &clockDeviceUID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioAggregateDevicePropertyClockDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return [[SFBAudioClockDevice alloc] initWithClockDeviceUID:(__bridge_transfer NSString *)clockDeviceUID];
}

- (BOOL)setClockDevice:(SFBAudioClockDevice *)clockDevice error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting aggregate device 0x%x clock device to %{public}@", self.deviceID, clockDevice.clockDeviceUID ?: @"nil");

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioAggregateDevicePropertyClockDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef clockDeviceUID = (__bridge CFStringRef)clockDevice.clockDeviceUID;
	OSStatus result = AudioObjectSetPropertyData(self.deviceID, &propertyAddress, 0, NULL, sizeof(clockDeviceUID), &clockDeviceUID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioAggregateDevicePropertyClockDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (BOOL)isPrivate
{
	return [[self.composition objectForKey:@ kAudioAggregateDeviceIsPrivateKey] boolValue];
}

- (BOOL)isStacked
{
	return [[self.composition objectForKey:@ kAudioAggregateDeviceIsStackedKey] boolValue];
}

@end
