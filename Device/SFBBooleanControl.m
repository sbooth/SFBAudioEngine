/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBBooleanControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBBooleanControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioBooleanControlClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSNumber *)value
{
	return [self unsignedIntForProperty:kAudioBooleanControlPropertyValue inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setValue:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioBooleanControlPropertyValue inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

@end

@implementation SFBMuteControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioMuteControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBSoloControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioSoloControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBJackControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioJackControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBLFEMuteControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioLFEMuteControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBPhantomPowerControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioPhantomPowerControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBPhaseInvertControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioPhaseInvertControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBClipLightControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioClipLightControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBTalkbackControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioTalkbackControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end

@implementation SFBListenbackControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioListenbackControlClassID));
	return [super initWithAudioObjectID:objectID];
}

@end
