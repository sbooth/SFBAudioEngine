/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBSubdevice.h"
#import "SFBAudioObject+Internal.h"

#import "NSArray+SFBFunctional.h"
#import "SFBCStringForOSType.h"

@implementation SFBSubdevice

+ (NSArray *)subdevices
{
	return [[SFBAudioDevice devices] filteredArrayUsingBlock:^BOOL(SFBAudioDevice *obj) {
		return [obj isSubdevice];
	}];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioSubDeviceClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSNumber *)extraLatency
{
	return [self doubleForProperty:kAudioSubDevicePropertyExtraLatency inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)driftCompensation
{
	return [self unsignedIntForProperty:kAudioSubDevicePropertyDriftCompensation inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setDriftCompensation:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioSubDevicePropertyDriftCompensation inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)driftCompensationQuality
{
	return [self unsignedIntForProperty:kAudioSubDevicePropertyDriftCompensationQuality inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setDriftCompensationQuality:(unsigned int)value error:(NSError **)error
{
	return [self setUnsignedInt:value forProperty:kAudioSubDevicePropertyDriftCompensationQuality inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

@end
