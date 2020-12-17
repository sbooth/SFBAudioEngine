/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBLevelControl.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBLevelControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioLevelControlClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSNumber *)scalarValue
{
	return [self floatForProperty:kAudioLevelControlPropertyScalarValue];
}

- (NSNumber *)decibelValue
{
	return [self floatForProperty:kAudioLevelControlPropertyDecibelValue];
}

- (NSValue *)decibelRange
{
	return [self audioValueRangeForProperty:kAudioLevelControlPropertyDecibelRange];
}

- (NSNumber *)convertToDecibelsFromScalar:(NSNumber *)scalar error:(NSError **)error
{
	NSParameterAssert(scalar != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioLevelControlPropertyConvertScalarToDecibels,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 value = scalar.floatValue;
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioLevelControlPropertyConvertScalarToDecibels) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return nil;
	}

	return @(value);
}

- (NSNumber *)convertToScalarFromDecibels:(NSNumber *)decibels error:(NSError **)error
{
	NSParameterAssert(decibels != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioLevelControlPropertyConvertDecibelsToScalar,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 value = decibels.floatValue;
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioLevelControlPropertyConvertDecibelsToScalar) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return nil;
	}

	return @(value);
}

@end
