/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBSelectorControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBSelectorControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioSelectorControlClassID));
	return [super initWithAudioObjectID:objectID];
}

- (NSArray *)currentItem
{
	return [self unsignedIntArrayForProperty:kAudioSelectorControlPropertyCurrentItem inScope:SFBAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setCurrentItem:(NSArray <NSNumber *> *)values error:(NSError **)error
{
	return [self setUnsignedIntArray:values forProperty:kAudioSelectorControlPropertyCurrentItem inScope:SFBAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSArray *)availableItems
{
	return [self unsignedIntArrayForProperty:kAudioSelectorControlPropertyAvailableItems inScope:SFBAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)nameOfItem:(UInt32)itemID
{
	return [self stringForProperty:kAudioSelectorControlPropertyItemName inScope:SFBAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:&itemID qualifierSize:sizeof(itemID) error:NULL];
}

- (NSString *)kindOfItem:(UInt32)itemID
{
	return [self stringForProperty:kAudioSelectorControlPropertyItemKind inScope:SFBAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:&itemID qualifierSize:sizeof(itemID) error:NULL];
}

@end
