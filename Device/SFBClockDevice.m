/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioClockDeviceClassID));
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

- (SFBAudioDeviceTransportType)transportType
{
	return [[self uintForProperty:kAudioDevicePropertyTransportType] unsignedIntValue];
}

- (UInt32)domain
{
	return [[self uintForProperty:kAudioDevicePropertyClockDomain] unsignedIntValue];
}

- (BOOL)isAlive
{
	return [[self uintForProperty:kAudioDevicePropertyDeviceIsAlive] boolValue];
}

- (BOOL)isRunning
{
	return [[self uintForProperty:kAudioDevicePropertyDeviceIsRunning] boolValue];
}

- (UInt32)latency
{
	return [[self uintForProperty:kAudioDevicePropertyLatency] unsignedIntValue];
}

- (NSArray *)controls
{
	return [self audioObjectArrayForProperty:kAudioObjectPropertyControlList];
}

- (UInt32)safetyOffset
{
	return [[self uintForProperty:kAudioDevicePropertySafetyOffset] unsignedIntValue];
}

- (double)sampleRate
{
	return [[self doubleForProperty:kAudioClockDevicePropertyNominalSampleRate] doubleValue];
}

- (NSArray *)availableSampleRates
{
	return [self audioValueRangeArrayForProperty:kAudioClockDevicePropertyAvailableNominalSampleRates];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.clockDeviceUID];
}

@end
