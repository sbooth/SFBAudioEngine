/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioStream.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

namespace {

	NSValue * _Nullable AudioStreamBasicDescriptionForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		AudioStreamBasicDescription value;
		return SFB::GetFixedSizeProperty(objectID, propertyAddress, value) ? [NSValue valueWithAudioStreamBasicDescription:value] : nil;
	}

	NSArray<NSValue *> * _Nullable AudioStreamRangedDescriptionArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		std::vector<AudioStreamRangedDescription> values;
		if(!SFB::GetArrayProperty(objectID, propertyAddress, values))
			return nil;
		NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
		for(AudioStreamRangedDescription value : values)
			[result addObject:[NSValue valueWithAudioStreamRangedDescription:value]];
		return result;
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

- (NSValue *)virtualFormatOnElement:(SFBAudioObjectPropertyElement)element
{
	return AudioStreamBasicDescriptionForProperty(_objectID, kAudioStreamPropertyVirtualFormat);
}

- (NSArray *)availableVirtualFormatsOnElement:(SFBAudioObjectPropertyElement)element
{
	return AudioStreamRangedDescriptionArrayForProperty(_objectID, kAudioStreamPropertyAvailableVirtualFormats);
}

- (NSValue *)physicalFormatOnElement:(SFBAudioObjectPropertyElement)element
{
	return AudioStreamBasicDescriptionForProperty(_objectID, kAudioStreamPropertyPhysicalFormat);
}

- (NSArray *)availablePhysicalFormatsOnElement:(SFBAudioObjectPropertyElement)element
{
	return AudioStreamRangedDescriptionArrayForProperty(_objectID, kAudioStreamPropertyAvailablePhysicalFormats);
}

@end

@implementation NSValue (AudioStreamBasicDescription)

+ (instancetype)valueWithAudioStreamBasicDescription:(AudioStreamBasicDescription)asbd
{
	return [NSValue value:&asbd withObjCType:@encode(AudioStreamBasicDescription)];
}

- (AudioStreamBasicDescription)audioStreamBasicDescriptionValue
{
	AudioStreamBasicDescription asbd;
	[self getValue:&asbd];
	return asbd;
}

@end

@implementation NSValue (AudioStreamRangedDescription)

+ (instancetype)valueWithAudioStreamRangedDescription:(AudioStreamRangedDescription)asrd
{
	return [NSValue value:&asrd withObjCType:@encode(AudioStreamRangedDescription)];
}

- (AudioStreamRangedDescription)audioStreamRangedDescriptionValue
{
	AudioStreamRangedDescription asrd;
	[self getValue:&asrd];
	return asrd;
}

@end
