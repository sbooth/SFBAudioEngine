/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioPlugIn.h"
#import "SFBAudioObject+Internal.h"

#import "SFBAudioBox.h"
#import "SFBAudioDevice.h"
#import "SFBClockDevice.h"
#import "SFBCStringForOSType.h"

@implementation SFBAudioPlugIn

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsPlugIn(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (NSString *)bundleID
{
	return [self stringForProperty:kAudioPlugInPropertyBundleID];
}

- (NSArray *)devices
{
	return [self audioObjectArrayForProperty:kAudioPlugInPropertyDeviceList];
}

- (NSArray *)boxes
{
	return [self audioObjectArrayForProperty:kAudioPlugInPropertyBoxList];
}

- (NSArray *)clocks
{
	return [self audioObjectArrayForProperty:kAudioPlugInPropertyClockDeviceList];
}

@end
