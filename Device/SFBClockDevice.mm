/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBClockDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

namespace {

	NSArray<NSValue *> * _Nullable AudioValueRangeArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		std::vector<AudioValueRange> values;
		if(!SFB::GetArrayProperty(objectID, propertyAddress, values))
			return nil;
		NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
		for(AudioValueRange value : values)
			[result addObject:[NSValue valueWithAudioValueRange:value]];
		return result;
	}

}

@implementation SFBClockDevice

+ (NSArray *)clockDevices
{
	return SFB::AudioObjectArrayForProperty(kAudioObjectSystemObject, kAudioHardwarePropertyClockDeviceList);
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
	return SFB::StringForProperty(_objectID, kAudioClockDevicePropertyDeviceUID);
}

- (SFBAudioDeviceTransportType)transportType
{
	return (SFBAudioDeviceTransportType)SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioDevicePropertyTransportType);
}

- (UInt32)domain
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioDevicePropertyClockDomain);
}

- (BOOL)isAlive
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioDevicePropertyDeviceIsAlive);
}

- (BOOL)isRunning
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioDevicePropertyDeviceIsRunning);
}

- (UInt32)latency
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioDevicePropertyLatency);
}

- (NSArray *)controls
{
	return SFB::AudioObjectArrayForProperty(_objectID, kAudioObjectPropertyControlList);
}

- (UInt32)safetyOffset
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioDevicePropertySafetyOffset);
}

- (double)sampleRate
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioClockDevicePropertyNominalSampleRate);
}

- (NSArray *)availableSampleRates
{
	return AudioValueRangeArrayForProperty(_objectID, kAudioStreamPropertyAvailableVirtualFormats);
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.clockDeviceUID];
}

@end
