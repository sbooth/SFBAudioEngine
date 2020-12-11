/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAggregateAudioDevice.h"

#import "SFBCStringForOSType.h"

extern os_log_t gSFBAudioDeviceLog;
extern BOOL SFBDeviceIsAggregate(AudioObjectID deviceID);

@implementation SFBAggregateAudioDevice

- (instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID
{
	NSParameterAssert(audioObjectID != kAudioObjectUnknown);
	NSParameterAssert(SFBDeviceIsAggregate(audioObjectID));

	return [super initWithAudioObjectID:audioObjectID];
}

- (BOOL)isPrivate
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioAggregateDevicePropertyComposition) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	return [[(__bridge_transfer NSDictionary *)composition objectForKey:@ kAudioAggregateDeviceIsPrivateKey] boolValue];
}

@end
