/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBEndpointDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBEndpointDevice

+ (NSArray *)endpointDevices
{
	NSArray *devices = [SFBAudioDevice devices];
	return [devices objectsAtIndexes:[devices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return [obj isEndpointDevice];
	}]];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioEndPointDeviceClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSDictionary *)compositionInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self dictionaryForProperty:kAudioEndPointDevicePropertyComposition inScope:scope onElement:element error:NULL];
}

- (NSArray *)endpointsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioObjectArrayForProperty:kAudioEndPointDevicePropertyEndPointList inScope:scope onElement:element error:NULL];
}

- (NSNumber *)isPrivateInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioEndPointDevicePropertyIsPrivate inScope:scope onElement:element error:NULL];
}

@end
