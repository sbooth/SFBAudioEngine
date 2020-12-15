/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioBox.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBAudioBox

+ (NSArray *)boxes
{
	return [[SFBAudioObject systemObject] audioObjectsForProperty:kAudioHardwarePropertyBoxList];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsBox(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (instancetype)initWithBoxUID:(NSString *)boxUID
{
	NSParameterAssert(boxUID != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyTranslateUIDToBox,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID deviceID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(deviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(boxUID), &boxUID, &specifierSize, &deviceID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyTranslateUIDToBox) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(deviceID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioObjectLog, "Unknown audio box UID: %{public}@", boxUID);
		return nil;
	}

	return [self initWithAudioObjectID:deviceID];
}

- (NSString *)boxUID
{
	return [self stringForProperty:kAudioBoxPropertyBoxUID];
}

- (SFBAudioDeviceTransportType)transportType
{
	return [[self uInt32ForProperty:kAudioBoxPropertyTransportType] unsignedIntValue];
}

- (BOOL)hasAudio
{
	return [[self uInt32ForProperty:kAudioBoxPropertyHasAudio] boolValue];
}

- (BOOL)hasVideo
{
	return [[self uInt32ForProperty:kAudioBoxPropertyHasVideo] boolValue];
}

- (BOOL)hasMIDI
{
	return [[self uInt32ForProperty:kAudioBoxPropertyHasMIDI] boolValue];
}

- (BOOL)acquired
{
	return [[self uInt32ForProperty:kAudioBoxPropertyAcquired] boolValue];
}

- (NSArray *)devices
{
	return [self audioObjectsForProperty:kAudioBoxPropertyDeviceList];
}

- (NSArray *)clockDevices
{
	return [self audioObjectsForProperty:kAudioBoxPropertyClockDeviceList];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.boxUID];
}

@end
