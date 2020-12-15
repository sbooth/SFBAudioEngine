/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBLevelControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBLevelControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioControlIsLevel(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (Float32)scalarValue
{
	return [[self float32ForProperty:kAudioLevelControlPropertyScalarValue] floatValue];
}

- (Float32)decibelValue
{
	return [[self float32ForProperty:kAudioLevelControlPropertyDecibelValue] floatValue];
}

@end
