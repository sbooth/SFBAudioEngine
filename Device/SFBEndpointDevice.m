/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBEndpointDevice.h"
#import "SFBAudioObject+Internal.h"

#import "NSArray+SFBFunctional.h"
#import "SFBCStringForOSType.h"

@implementation SFBEndpointDevice

+ (NSArray *)endpointDevices
{
	return [[SFBAudioDevice devices] filteredArrayUsingBlock:^BOOL(SFBAudioDevice *obj) {
		return [obj isEndpointDevice];
	}];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioEndPointDeviceClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSDictionary *)composition
{
	return [self dictionaryForProperty:kAudioEndPointDevicePropertyComposition inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)endpoints
{
	return [self audioObjectArrayForProperty:kAudioEndPointDevicePropertyEndPointList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isPrivate
{
	return [self unsignedIntForProperty:kAudioEndPointDevicePropertyIsPrivate inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

@end
