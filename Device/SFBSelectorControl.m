/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBSelectorControl.h"
#import "SFBAudioObject+Internal.h"

@implementation SFBSelectorControl

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioControlIsSelector(objectID));
	return [super initWithAudioObjectID:objectID];
}

- (NSArray *)currentItem
{
	return [self uInt32ArrayForProperty:kAudioSelectorControlPropertyCurrentItem];
}

- (NSArray *)availableItems
{
	return [self uInt32ArrayForProperty:kAudioSelectorControlPropertyAvailableItems];
}

- (NSString *)itemName
{
	return [self stringForProperty:kAudioSelectorControlPropertyItemName];
}

- (UInt32)itemKind
{
	return [[self uInt32ForProperty:kAudioSelectorControlPropertyItemKind] unsignedIntValue];
}

@end
