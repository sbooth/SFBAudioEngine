/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBEndpointDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBEndpointDevice

+ (NSArray *)endpointDevices
{
	NSArray *devices = [SFBAudioDevice devices];
	return [devices objectsAtIndexes:[devices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return [obj isEndpointDevice];
	}]];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioDeviceIsEndpointDevice(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (NSDictionary *)composition
{
	return [self dictionaryForProperty:kAudioEndPointDevicePropertyComposition];
}

- (NSArray *)endPoints
{
	return [self audioObjectArrayForProperty:kAudioEndPointDevicePropertyEndPointList];
}

- (pid_t)isPrivate
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioEndPointDevicePropertyIsPrivate,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	pid_t pid = 0;
	UInt32 dataSize = sizeof(pid);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &pid);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioEndPointDevicePropertyIsPrivate) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return 0;
	}

	return pid;
}

@end
