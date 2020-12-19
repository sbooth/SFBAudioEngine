/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioStream.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioStream

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioStreamClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSNumber *)isActiveOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioStreamPropertyIsActive inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSNumber *)isOutputOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioStreamPropertyDirection inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSNumber *)terminalTypeOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioStreamPropertyTerminalType inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSNumber *)startingChannelOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioStreamPropertyStartingChannel inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSNumber *)latencyOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioStreamPropertyLatency inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSValue *)virtualFormatOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamBasicDescriptionForProperty:kAudioStreamPropertyVirtualFormat];
}

- (BOOL)setVirtualFormat:(AudioStreamBasicDescription)value onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setAudioStreamBasicDescription:value forProperty:kAudioStreamPropertyVirtualFormat inScope:kAudioObjectPropertyScopeGlobal onElement:element error:error];
}

- (NSArray *)availableVirtualFormatsOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamRangedDescriptionArrayForProperty:kAudioStreamPropertyAvailableVirtualFormats];
}

- (NSValue *)physicalFormatOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamBasicDescriptionForProperty:kAudioStreamPropertyPhysicalFormat];
}

- (BOOL)setPhysicalFormat:(AudioStreamBasicDescription)value onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setAudioStreamBasicDescription:value forProperty:kAudioStreamPropertyPhysicalFormat inScope:kAudioObjectPropertyScopeGlobal onElement:element error:error];
}

- (NSArray *)availablePhysicalFormatsOnElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioStreamRangedDescriptionArrayForProperty:kAudioStreamPropertyAvailablePhysicalFormats];
}

@end
