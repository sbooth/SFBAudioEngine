/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
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
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioTransportManagerClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSArray *)endPoints
{
	return [self audioObjectArrayForProperty:kAudioTransportManagerPropertyEndPointList];
}

- (SFBAudioDeviceTransportType)transportType
{
	return [[self uintForProperty:kAudioTransportManagerPropertyTransportType] unsignedIntValue];
}

@end
