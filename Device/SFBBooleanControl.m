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
