/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAggregateDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBClockDevice.h"
#import "SFBCStringForOSType.h"

@implementation SFBAggregateDevice

+ (NSArray *)aggregateDevices
{
	NSArray *devices = [SFBAudioDevice devices];
	return [devices objectsAtIndexes:[devices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return [obj isAggregate];
	}]];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioAggregateDeviceClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSArray *)allSubdevicesInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self arrayForProperty:kAudioAggregateDevicePropertyFullSubDeviceList inScope:scope onElement:element error:NULL];
}

- (NSArray *)activeSubdevicesInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioObjectArrayForProperty:kAudioAggregateDevicePropertyActiveSubDeviceList inScope:scope onElement:element error:NULL];
}

- (NSDictionary *)compositionInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self dictionaryForProperty:kAudioAggregateDevicePropertyComposition inScope:scope onElement:element error:NULL];
}

- (SFBAudioDevice *)masterSubdeviceInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return (SFBAudioDevice *)[self audioObjectForProperty:kAudioAggregateDevicePropertyMasterSubDevice inScope:scope onElement:element error:NULL];
}

- (SFBClockDevice *)clockDeviceInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return (SFBClockDevice *)[self audioObjectForProperty:kAudioAggregateDevicePropertyClockDevice inScope:scope onElement:element error:NULL];
}

- (BOOL)setClockDevice:(SFBClockDevice *)clockDevice inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting aggregate device 0x%x clock device ('%{public}.4s', %u) to %{public}@", _objectID, SFBCStringForOSType(scope), element, clockDevice.clockDeviceUID ?: @"nil");
	return [self setAudioObject:clockDevice forProperty:kAudioAggregateDevicePropertyClockDevice inScope:scope onElement:element error:error];
}

- (NSNumber *)isPrivateInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [[self compositionInScope:scope onElement:element] objectForKey:@ kAudioAggregateDeviceIsPrivateKey];
}

- (NSNumber *)isStackedInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [[self compositionInScope:scope onElement:element] objectForKey:@ kAudioAggregateDeviceIsStackedKey];
}

@end
