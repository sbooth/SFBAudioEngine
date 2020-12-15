/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBSubdevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBSubdevice

+ (NSArray *)subdevices
{
	NSArray *devices = [SFBAudioDevice devices];
	return [devices objectsAtIndexes:[devices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return [obj isSubdevice];
	}]];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioDeviceIsSubdevice(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (Float64)extraLatency
{
	return [[self float64ForProperty:kAudioSubDevicePropertyExtraLatency] doubleValue];
}

- (UInt32)driftCompensation
{
	return [[self uInt32ForProperty:kAudioSubDevicePropertyDriftCompensation] unsignedIntValue];
}

- (UInt32)driftCompensationQuality
{
	return [[self uInt32ForProperty:kAudioSubDevicePropertyDriftCompensationQuality] unsignedIntValue];
}

@end
