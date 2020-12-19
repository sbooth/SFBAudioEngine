/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioSubDeviceClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSNumber *)extraLatencyInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self doubleForProperty:kAudioSubDevicePropertyExtraLatency inScope:scope onElement:element error:NULL];
}

- (NSNumber *)driftCompensationInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioSubDevicePropertyDriftCompensation inScope:scope onElement:element error:NULL];
}

- (BOOL)setDriftCompensation:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioSubDevicePropertyDriftCompensation inScope:scope onElement:element error:error];
}

- (NSNumber *)driftCompensationQualityInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioSubDevicePropertyDriftCompensationQuality inScope:scope onElement:element error:NULL];
}

- (BOOL)setDriftCompensationQuality:(unsigned int)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:value forProperty:kAudioSubDevicePropertyDriftCompensationQuality inScope:scope onElement:element error:error];
}

@end
