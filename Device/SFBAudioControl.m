/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioControlClassID));
	return [super initWithAudioObjectID:objectID];
}

- (SFBAudioObjectPropertyScope)scope
{
	return [[self uintForProperty:kAudioControlPropertyScope] unsignedIntValue];
}

- (SFBAudioObjectPropertyElement)element
{
	return [[self uintForProperty:kAudioControlPropertyElement] unsignedIntValue];
}

@end
