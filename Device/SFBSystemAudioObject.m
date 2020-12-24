/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBSystemAudioObject.h"
#import "SFBAudioObject+Internal.h"

@interface SFBSystemAudioObject ()
- (instancetype)init;
@end

@implementation SFBSystemAudioObject

static SFBSystemAudioObject *sSharedInstance = nil;

+ (SFBSystemAudioObject *)sharedInstance
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sSharedInstance = [[SFBSystemAudioObject alloc] init];
	});
	return sSharedInstance;
}

- (instancetype)init
{
	if((self = [super init]))
		_objectID = kAudioObjectSystemObject;
	return self;
}

- (NSNumber *)mixStereoToMono
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyMixStereoToMono inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setMixStereoToMono:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioHardwarePropertyMixStereoToMono inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)processIsMaster
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyProcessIsMaster inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isInitingOrExiting
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyIsInitingOrExiting inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setUserIDChangedReturningError:(NSError **)error
{
	return [self setUnsignedInt:1 forProperty:kAudioHardwarePropertyUserIDChanged inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)processIsAudible
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyProcessIsAudible inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setProcessIsAudible:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioHardwarePropertyProcessIsAudible inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)sleepingIsAllowed
{
	return [self unsignedIntForProperty:kAudioHardwarePropertySleepingIsAllowed inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setSleepingIsAllowed:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioHardwarePropertySleepingIsAllowed inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)unloadingIsAllowed
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyUnloadingIsAllowed inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setUnloadingIsAllowed:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioHardwarePropertyUnloadingIsAllowed inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)hogModeIsAllowed
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyHogModeIsAllowed inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setHogModeIsAllowed:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioHardwarePropertyHogModeIsAllowed inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)userSessionIsActiveOrHeadless
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyUserSessionIsActiveOrHeadless inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)powerHint
{
	return [self unsignedIntForProperty:kAudioHardwarePropertyPowerHint inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setPowerHint:(AudioHardwarePowerHint)value error:(NSError **)error
{
	return [self setUnsignedInt:value forProperty:kAudioHardwarePropertyPowerHint inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

@end
