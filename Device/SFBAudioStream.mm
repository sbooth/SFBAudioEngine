/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioStream.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBAudioStream

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsStream(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (BOOL)isActive
{
	return [[self uintForProperty:kAudioStreamPropertyIsActive] boolValue];
}

- (BOOL)isOutput
{
	return [[self uintForProperty:kAudioStreamPropertyDirection] boolValue];
}

- (SFBAudioStreamTerminalType)terminalType
{
	return [[self uintForProperty:kAudioStreamPropertyTerminalType] unsignedIntValue];
}

- (UInt32)startingChannel
{
	return [[self uintForProperty:kAudioStreamPropertyStartingChannel] unsignedIntValue];
}

- (UInt32)latency
{
	return [[self uintForProperty:kAudioStreamPropertyLatency] unsignedIntValue];
}

- (BOOL)getVirtualFormat:(AudioStreamBasicDescription *)streamDescription
{
	NSParameterAssert(streamDescription != NULL);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioStreamPropertyVirtualFormat,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioStreamBasicDescription asbd = {0};
	UInt32 dataSize = sizeof(asbd);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &asbd);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	return YES;
}

- (BOOL)getVirtualFormat:(AudioStreamBasicDescription *)streamDescription onElement:(SFBAudioObjectPropertyElement)element
{
	
}

- (AVAudioFormat *)physicalFormat
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioStreamPropertyPhysicalFormat,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioStreamBasicDescription asbd = {0};
	UInt32 dataSize = sizeof(asbd);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &asbd);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioStreamPropertyVirtualFormat) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return [[AVAudioFormat alloc] initWithStreamDescription:&asbd];
}

@end
