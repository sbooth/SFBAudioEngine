/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBSubdevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBSubdevice

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioDeviceIsSubdevice(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (Float64)extraLatency
{
	return [self float64ForProperty:kAudioSubDevicePropertyExtraLatency];
}

- (UInt32)driftCompensation
{
	return [self uInt32ForProperty:kAudioSubDevicePropertyDriftCompensation];
}

- (UInt32)driftCompensationQuality
{
	return [self uInt32ForProperty:kAudioSubDevicePropertyDriftCompensationQuality];
}

@end
