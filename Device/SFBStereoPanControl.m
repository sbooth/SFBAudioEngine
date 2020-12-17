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

- (float)value
{
	return [[self floatForProperty:kAudioStereoPanControlPropertyValue] floatValue];
}

- (NSArray *)panningChannels
{
	return [self uintArrayForProperty:kAudioStereoPanControlPropertyPanningChannels];
}

@end
