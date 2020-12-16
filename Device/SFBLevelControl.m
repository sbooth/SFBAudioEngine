/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBLevelControl.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBLevelControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioControlIsLevel(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (Float32)scalarValue
{
	return [[self float32ForProperty:kAudioLevelControlPropertyScalarValue] floatValue];
}

- (Float32)decibelValue
{
	return [[self float32ForProperty:kAudioLevelControlPropertyDecibelValue] floatValue];
}

- (NSArray *)decibelRange
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioLevelControlPropertyDecibelRange,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioValueRange value = {0};
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioLevelControlPropertyDecibelRange) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return [NSArray arrayWithObjects:@(value.mMinimum), @(value.mMaximum), nil];
}

- (Float32)convertToDecibelsFromScalar:(Float32)scalar
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioLevelControlPropertyConvertScalarToDecibels,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 value = scalar;
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioLevelControlPropertyConvertScalarToDecibels) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nanf("1");
	}

	return value;
}

- (Float32)convertToScalarFromDecibels:(Float32)decibels
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioLevelControlPropertyConvertDecibelsToScalar,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 value = decibels;
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioLevelControlPropertyConvertDecibelsToScalar) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nanf("1");
	}

	return value;
}

@end
