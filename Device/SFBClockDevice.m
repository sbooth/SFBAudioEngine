/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBClockDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBClockDevice

+ (NSArray *)clockDevices
{
	return [[SFBAudioObject systemObject] audioObjectsForProperty:kAudioHardwarePropertyClockDeviceList];
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

- (SFBCADeviceTransportType)transportType
{
	return [self uInt32ForProperty:kAudioDevicePropertyTransportType];
}

- (UInt32)domain
{
	return [self uInt32ForProperty:kAudioDevicePropertyClockDomain];
}

- (BOOL)isAlive
{
	return (BOOL)[self uInt32ForProperty:kAudioDevicePropertyDeviceIsAlive];
}

- (BOOL)isRunning
{
	return (BOOL)[self uInt32ForProperty:kAudioDevicePropertyDeviceIsRunning];
}

- (UInt32)latency
{
	return [self uInt32ForProperty:kAudioDevicePropertyLatency];
}

- (NSArray *)controls
{
	return [self audioObjectsForProperty:kAudioObjectPropertyControlList];
}

- (UInt32)safetyOffset
{
	return [self uInt32ForProperty:kAudioDevicePropertySafetyOffset];
}

- (double)sampleRate
{
	return [self float64ForProperty:kAudioClockDevicePropertyNominalSampleRate];
}

- (NSArray *)availableSampleRates
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioClockDevicePropertyAvailableNominalSampleRates,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_objectID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioValueRange *availableNominalSampleRates = (AudioValueRange *)malloc(dataSize);
	if(!availableNominalSampleRates) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, availableNominalSampleRates);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(availableNominalSampleRates);
		return nil;
	}

	NSMutableArray *availablSampleRates = [NSMutableArray array];

	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioValueRange)); ++i) {
		AudioValueRange nominalSampleRate = availableNominalSampleRates[i];
		if(nominalSampleRate.mMinimum == nominalSampleRate.mMaximum)
			[availablSampleRates addObject:@(nominalSampleRate.mMinimum)];
		else
			os_log_error(gSFBAudioObjectLog, "nominalSampleRate.mMinimum (%.2f Hz) and nominalSampleRate.mMaximum (%.2f Hz) don't match", nominalSampleRate.mMinimum, nominalSampleRate.mMaximum);
	}

	free(availableNominalSampleRates);

	return availablSampleRates;
}
- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.clockDeviceUID];
}

@end
