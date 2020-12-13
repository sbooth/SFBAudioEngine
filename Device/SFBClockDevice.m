/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBClockDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBClockDevice

+ (NSArray *)clockDevices
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyClockDeviceList];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClockDevice(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (instancetype)initWithClockDeviceUID:(NSString *)clockDeviceUID
{
	NSParameterAssert(clockDeviceUID != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyTranslateUIDToClockDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID clockDeviceID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(clockDeviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(clockDeviceUID), &clockDeviceUID, &specifierSize, &clockDeviceID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyTranslateUIDToClockDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(clockDeviceID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioObjectLog, "Unknown audio clock device UID: %{public}@", clockDeviceUID);
		return nil;
	}

	return [self initWithAudioObjectID:clockDeviceID];
}

- (NSString *)clockDeviceUID
{
	return [self stringForProperty:kAudioClockDevicePropertyDeviceUID];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.clockDeviceUID];
}

@end
