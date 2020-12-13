/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioBox.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBAudioBox

+ (NSArray *)boxes
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyBoxList];
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

- (UInt32)transportType
{
	return [self uInt32ForProperty:kAudioBoxPropertyTransportType];
}

- (BOOL)hasAudio
{
	return (BOOL)[self uInt32ForProperty:kAudioBoxPropertyHasAudio];
}

- (BOOL)hasVideo
{
	return (BOOL)[self uInt32ForProperty:kAudioBoxPropertyHasVideo];
}

- (BOOL)hasMIDI
{
	return (BOOL)[self uInt32ForProperty:kAudioBoxPropertyHasMIDI];
}

- (BOOL)acquired
{
	return (BOOL)[self uInt32ForProperty:kAudioBoxPropertyAcquired];
}

- (NSArray *)devices
{
	return [self audioObjectArrayForProperty:kAudioBoxPropertyDeviceList];
}

- (NSArray *)clocks
{
	return [self audioObjectArrayForProperty:kAudioBoxPropertyClockDeviceList];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.boxUID];
}

@end
