/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBStereoPanControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBStereoPanControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioControlIsStereoPan(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (Float32)value
{
	return [[self float32ForProperty:kAudioStereoPanControlPropertyValue] floatValue];
}

- (NSArray *)panningChannels
{
	return [self uInt32ArrayForProperty:kAudioStereoPanControlPropertyPanningChannels];
}

@end
