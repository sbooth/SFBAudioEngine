/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioPlugIn.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioPlugIn

+ (NSArray *)plugIns
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyPlugInList];
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
