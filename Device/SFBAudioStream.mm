/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioStream.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

namespace {

	bool AudioStreamBasicDescriptionForProperty(AudioObjectID objectID, AudioStreamBasicDescription& streamDescription, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return SFB::GetFixedSizeProperty(objectID, propertyAddress, streamDescription);
	}

	bool AudioStreamBasicDescriptionArrayForProperty(AudioObjectID objectID, std::vector<AudioStreamBasicDescription>& streamDescriptions, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return SFB::GetArrayProperty(objectID, propertyAddress, streamDescriptions);
	}

}

@implementation SFBAudioStream

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsStream(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (BOOL)isActive
{
	return [SFB::UInt32ForProperty(_objectID, kAudioStreamPropertyIsActive) boolValue];
}

- (BOOL)isOutput
{
	return [SFB::UInt32ForProperty(_objectID, kAudioStreamPropertyDirection) boolValue];
}

- (SFBAudioStreamTerminalType)terminalType
{
	return (SFBAudioStreamTerminalType)[SFB::UInt32ForProperty(_objectID, kAudioStreamPropertyTerminalType) unsignedIntValue];
}

- (UInt32)startingChannel
{
	return [SFB::UInt32ForProperty(_objectID, kAudioStreamPropertyStartingChannel) unsignedIntValue];
}

- (UInt32)latency
{
	return [SFB::UInt32ForProperty(_objectID, kAudioStreamPropertyLatency) unsignedIntValue];
}

- (BOOL)getVirtualFormat:(AudioStreamBasicDescription *)format
{
	NSParameterAssert(format != NULL);
	return AudioStreamBasicDescriptionForProperty(_objectID, *format, kAudioStreamPropertyVirtualFormat);
}

- (BOOL)getVirtualFormat:(AudioStreamBasicDescription *)format onElement:(SFBAudioObjectPropertyElement)element
{
	NSParameterAssert(format != NULL);
	return AudioStreamBasicDescriptionForProperty(_objectID, *format, kAudioStreamPropertyVirtualFormat, element);
}

- (BOOL)getPhysicalFormat:(AudioStreamBasicDescription *)format
{
	NSParameterAssert(format != NULL);
	return AudioStreamBasicDescriptionForProperty(_objectID, *format, kAudioStreamPropertyPhysicalFormat);
}

- (BOOL)getPhysicalFormat:(AudioStreamBasicDescription *)format  onElement:(SFBAudioObjectPropertyElement)element
{
	NSParameterAssert(format != NULL);
	return AudioStreamBasicDescriptionForProperty(_objectID, *format, kAudioStreamPropertyPhysicalFormat, element);
}

@end
