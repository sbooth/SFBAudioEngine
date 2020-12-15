/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBBooleanControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBBooleanControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioControlIsBoolean(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (BOOL)value
{
	return [[self uInt32ForProperty:kAudioSliderControlPropertyValue] boolValue];
}

@end
