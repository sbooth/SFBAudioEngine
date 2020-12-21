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

- (NSArray *)allSubdevices
{
	return [self arrayForProperty:kAudioAggregateDevicePropertyFullSubDeviceList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)activeSubdevices
{
	return [self audioObjectArrayForProperty:kAudioAggregateDevicePropertyActiveSubDeviceList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSDictionary *)composition
{
	return [self dictionaryForProperty:kAudioAggregateDevicePropertyComposition inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioDevice *)masterSubdevice
{
	return (SFBAudioDevice *)[self audioObjectForProperty:kAudioAggregateDevicePropertyMasterSubDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBClockDevice *)clockDevice
{
	return (SFBClockDevice *)[self audioObjectForProperty:kAudioAggregateDevicePropertyClockDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setClockDevice:(SFBClockDevice *)clockDevice error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting aggregate device 0x%x clock device to %{public}@", _objectID, clockDevice.clockDeviceUID ?: @"nil");
	return [self setAudioObject:clockDevice forProperty:kAudioAggregateDevicePropertyClockDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)isPrivate
{
	return [self.composition objectForKey:@ kAudioAggregateDeviceIsPrivateKey];
}

- (NSNumber *)isStacked
{
	return [self.composition objectForKey:@ kAudioAggregateDeviceIsStackedKey];
}

@end
