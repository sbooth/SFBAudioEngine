/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioTransportManager.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBAudioTransportManager

+ (NSArray *)transportManagers
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyTransportManagerList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioTransportManagerClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSArray *)endPoints
{
	return [self audioObjectArrayForProperty:kAudioTransportManagerPropertyEndPointList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)transportType
{
	return [self unsignedIntForProperty:kAudioTransportManagerPropertyTransportType inScope:SFBAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

@end
