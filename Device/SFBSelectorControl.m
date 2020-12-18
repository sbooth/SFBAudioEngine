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
	return [self unsignedIntArrayForProperty:kAudioSelectorControlPropertyCurrentItem];
}

- (NSArray *)availableItems
{
	return [self unsignedIntArrayForProperty:kAudioSelectorControlPropertyAvailableItems];
}

- (NSString *)itemName
{
	return [self stringForProperty:kAudioSelectorControlPropertyItemName];
}

- (NSNumber *)itemKind
{
	return [self unsignedIntForProperty:kAudioSelectorControlPropertyItemKind];
}

@end
