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

- (BOOL)isActiveOnElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioStreamPropertyIsActive, kAudioObjectPropertyScopeGlobal, element);
}

- (BOOL)isOutputOnElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioStreamPropertyDirection, kAudioObjectPropertyScopeGlobal, element);
}

- (SFBAudioStreamTerminalType)terminalTypeOnElement:(SFBAudioObjectPropertyElement)element
{
	return (SFBAudioStreamTerminalType)SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioStreamPropertyTerminalType, kAudioObjectPropertyScopeGlobal, element);
}

- (UInt32)startingChannelOnElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioStreamPropertyStartingChannel, kAudioObjectPropertyScopeGlobal, element);
}

- (UInt32)latencyOnElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::NumericTypeForProperty<UInt32>(_objectID, kAudioStreamPropertyLatency, kAudioObjectPropertyScopeGlobal, element);
}

- (BOOL)getVirtualFormat:(AudioStreamBasicDescription *)format onElement:(SFBAudioObjectPropertyElement)element
{
	NSParameterAssert(format != NULL);
	return AudioStreamBasicDescriptionForProperty(_objectID, *format, kAudioStreamPropertyVirtualFormat, element);
}

- (BOOL)getPhysicalFormat:(AudioStreamBasicDescription *)format  onElement:(SFBAudioObjectPropertyElement)element
{
	NSParameterAssert(format != NULL);
	return AudioStreamBasicDescriptionForProperty(_objectID, *format, kAudioStreamPropertyPhysicalFormat, element);
}

@end
