/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsControl(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (SFBAudioObjectPropertyScope)scope
{
	return [[self uInt32ForProperty:kAudioControlPropertyScope] unsignedIntValue];
}

- (SFBAudioObjectPropertyElement)element
{
	return [[self uInt32ForProperty:kAudioControlPropertyElement] unsignedIntValue];
}

@end
