/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBSliderControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBSliderControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioSliderControlClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSNumber *)value
{
	return [self unsignedIntForProperty:kAudioSliderControlPropertyValue inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setValue:(unsigned int)value error:(NSError **)error
{
	return [self setUnsignedInt:value forProperty:kAudioSliderControlPropertyValue inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSArray *)range
{
	return [self unsignedIntArrayForProperty:kAudioSliderControlPropertyRange inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

@end
