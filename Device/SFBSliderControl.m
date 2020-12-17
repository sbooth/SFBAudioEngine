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

- (UInt32)value
{
	return [[self uintForProperty:kAudioSliderControlPropertyValue] unsignedIntValue];
}

- (NSArray *)range
{
	return [self uintArrayForProperty:kAudioSliderControlPropertyRange];
}

@end
