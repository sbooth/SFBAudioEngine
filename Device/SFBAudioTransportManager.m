/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioTransportManager.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioTransportManager

+ (NSArray *)transportManagers
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyTransportManagerList];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioPlugInIsTransportManager(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (NSArray *)endPoints
{
	return [self audioObjectArrayForProperty:kAudioTransportManagerPropertyEndPointList];
}

- (UInt32)transportType
{
	return [self uInt32ForProperty:kAudioTransportManagerPropertyTransportType];
}

@end
