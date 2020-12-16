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

- (double)extraLatency
{
	return [[self doubleForProperty:kAudioSubDevicePropertyExtraLatency] doubleValue];
}

- (BOOL)driftCompensation
{
	return [[self uintForProperty:kAudioSubDevicePropertyDriftCompensation] boolValue];
}

- (SFBSubdeviceDriftCompensationQuality)driftCompensationQuality
{
	return [[self uintForProperty:kAudioSubDevicePropertyDriftCompensationQuality] unsignedIntValue];
}

@end
