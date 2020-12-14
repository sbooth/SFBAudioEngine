/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioPlugIn.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioPlugIn

+ (NSArray *)plugIns
{
	return [[SFBAudioObject systemObject] audioObjectsForProperty:kAudioHardwarePropertyPlugInList];
}

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
	return [self audioObjectsForProperty:kAudioPlugInPropertyDeviceList];
}

- (NSArray *)boxes
{
	return [self audioObjectsForProperty:kAudioPlugInPropertyBoxList];
}

- (NSArray *)clockDevices
{
	return [self audioObjectsForProperty:kAudioPlugInPropertyClockDeviceList];
}

@end
