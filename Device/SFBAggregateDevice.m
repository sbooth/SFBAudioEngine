/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAggregateDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBClockDevice.h"
#import "SFBCStringForOSType.h"

@implementation SFBAggregateDevice

+ (NSArray *)aggregateDevices
{
	NSArray *devices = [SFBAudioDevice devices];
	return [devices objectsAtIndexes:[devices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return [obj isAggregate];
	}]];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID
{
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
	return [self audioObjectArrayForProperty:kAudioAggregateDevicePropertyActiveSubDeviceList];
}

- (NSDictionary *)composition
{
	return [self dictionaryForProperty:kAudioAggregateDevicePropertyComposition];
}

- (SFBAudioDevice *)masterSubdevice
{
	return (SFBAudioDevice *)[self audioObjectForProperty:kAudioAggregateDevicePropertyMasterSubDevice];
}

- (SFBClockDevice *)clockDevice
{
	return (SFBClockDevice *)[self audioObjectForProperty:kAudioAggregateDevicePropertyClockDevice];
}

- (BOOL)setClockDevice:(SFBClockDevice *)clockDevice error:(NSError **)error
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
