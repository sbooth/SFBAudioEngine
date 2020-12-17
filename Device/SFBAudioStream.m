/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioStream.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioStream

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsStream(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (BOOL)isActiveOnElement:(SFBAudioObjectPropertyElement)element
{
	return [[self uintForProperty:kAudioStreamPropertyIsActive inScope:kAudioObjectPropertyScopeGlobal onElement:element] boolValue];
}

- (BOOL)isOutputOnElement:(SFBAudioObjectPropertyElement)element
{
	return [[self uintForProperty:kAudioStreamPropertyDirection inScope:kAudioObjectPropertyScopeGlobal onElement:element] boolValue];
}

- (SFBAudioStreamTerminalType)terminalTypeOnElement:(SFBAudioObjectPropertyElement)element
{
	return [[self uintForProperty:kAudioStreamPropertyTerminalType inScope:kAudioObjectPropertyScopeGlobal onElement:element] unsignedIntValue];
}

- (UInt32)startingChannelOnElement:(SFBAudioObjectPropertyElement)element
{
	return [[self uintForProperty:kAudioStreamPropertyStartingChannel inScope:kAudioObjectPropertyScopeGlobal onElement:element] unsignedIntValue];
}

- (UInt32)latencyOnElement:(SFBAudioObjectPropertyElement)element
{
	return [[self uintForProperty:kAudioStreamPropertyLatency inScope:kAudioObjectPropertyScopeGlobal onElement:element] unsignedIntValue];
}

- (NSValue *)virtualFormatOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamBasicDescriptionForProperty:kAudioStreamPropertyVirtualFormat];
}

- (NSArray *)availableVirtualFormatsOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamRangedDescriptionArrayForProperty:kAudioStreamPropertyAvailableVirtualFormats];
}

- (NSValue *)physicalFormatOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamBasicDescriptionForProperty:kAudioStreamPropertyPhysicalFormat];
}

- (NSArray *)availablePhysicalFormatsOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamRangedDescriptionArrayForProperty:kAudioStreamPropertyAvailablePhysicalFormats];
}

@end
