/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBSliderControl.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBSliderControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioControlIsSlider(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (UInt32)value
{
	return [[self uInt32ForProperty:kAudioSliderControlPropertyValue] unsignedIntValue];
}

- (NSArray *)range
{
//	kAudioSliderControlPropertyRange
	return nil;
}

@end
