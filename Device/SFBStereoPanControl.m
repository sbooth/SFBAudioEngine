/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBStereoPanControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBStereoPanControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioStereoPanControlClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSNumber *)value
{
	return [self floatForProperty:kAudioStereoPanControlPropertyValue];
}

- (BOOL)setValue:(float)value error:(NSError **)error
{
	return [self setFloat:value forProperty:kAudioStereoPanControlPropertyValue inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSArray *)panningChannels
{
	return [self unsignedIntArrayForProperty:kAudioStereoPanControlPropertyPanningChannels];
}

@end
