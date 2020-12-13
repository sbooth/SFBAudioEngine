/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBEndPointDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBEndPointDevice

+ (NSArray *)endPointDevices
{
	NSMutableArray *endPointDevices = [NSMutableArray array];

	NSArray *devices = [SFBAudioDevice devices];
	for(SFBAudioDevice *device in devices) {
		if(device.isEndPoint)
			[endPointDevices addObject:device];
	}

	return endPointDevices;
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioDeviceIsEndPoint(objectID));
	return [super initWithAudioObjectID:objectID];
}

@end
