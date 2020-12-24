/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBClockDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBSystemAudioObject.h"
#import "SFBCStringForOSType.h"

@implementation SFBClockDevice

+ (NSArray *)clockDevices
{
	return [[SFBSystemAudioObject sharedInstance] audioObjectArrayForProperty:kAudioHardwarePropertyClockDeviceList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
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

	AudioObjectID objectID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(objectID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(clockDeviceUID), &clockDeviceUID, &specifierSize, &objectID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyTranslateUIDToClockDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(objectID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioObjectLog, "Unknown audio clock device UID: %{public}@", clockDeviceUID);
		return nil;
	}

	return [self initWithAudioObjectID:objectID];
}

- (NSString *)clockDeviceUID
{
	return [self stringForProperty:kAudioClockDevicePropertyDeviceUID inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)transportType
{
	return [self unsignedIntForProperty:kAudioClockDevicePropertyTransportType inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)domain
{
	return [self unsignedIntForProperty:kAudioClockDevicePropertyClockDomain inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isAlive
{
	return [self unsignedIntForProperty:kAudioClockDevicePropertyDeviceIsAlive inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isRunning
{
	return [self unsignedIntForProperty:kAudioClockDevicePropertyDeviceIsRunning inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)latency
{
	return [self unsignedIntForProperty:kAudioClockDevicePropertyLatency inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)controls
{
	return [self audioObjectArrayForProperty:kAudioClockDevicePropertyControlList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)sampleRate
{
	return [self doubleForProperty:kAudioClockDevicePropertyNominalSampleRate inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)availableSampleRates
{
	return [self audioValueRangeArrayForProperty:kAudioClockDevicePropertyAvailableNominalSampleRates inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.clockDeviceUID];
}

@end
