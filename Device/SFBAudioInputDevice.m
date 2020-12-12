/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioInputDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBAudioInputDevice

+ (NSArray *)inputDevices
{
	NSMutableArray *inputDevices = [NSMutableArray array];

	NSArray *devices = [SFBAudioDevice devices];
	for(SFBAudioDevice *device in devices) {
		if(device.supportsInput) {
			SFBAudioInputDevice *inputDevice = [[SFBAudioInputDevice alloc] initWithAudioObjectID:device.deviceID];
			if(inputDevice)
				[inputDevices addObject:inputDevice];
		}
	}

	return inputDevices;
}

+ (SFBAudioInputDevice *)defaultInputDevice
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyDefaultInputDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID deviceID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(deviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &specifierSize, &deviceID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultInputDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return [[SFBAudioInputDevice alloc] initWithAudioObjectID:deviceID];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID
{
	NSParameterAssert(audioObjectID != kAudioObjectUnknown);
	NSParameterAssert(SFBAudioDeviceSupportsInput(audioObjectID));

	return [super initWithAudioObjectID:audioObjectID];
}

@end
