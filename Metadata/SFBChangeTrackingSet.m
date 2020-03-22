/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBChangeTrackingSet.h"

@interface SFBChangeTrackingSet ()
{
@private
	NSSet *_initial;
	NSMutableSet *_added;
	NSMutableSet *_removed;
}
@end

@implementation SFBChangeTrackingSet

- (instancetype)init
{
	if((self = [super init])) {
		_initial = [NSSet set];
		_added = [NSMutableSet set];
		_removed = [NSMutableSet set];
	}
	return self;
}

- (instancetype)initWithInitialObjects:(NSArray *)initialObjects
{
	if((self = [super init])) {
		_initial = [NSSet setWithArray:initialObjects];
		_added = [NSMutableSet set];
		_removed = [NSMutableSet set];
	}
	return self;
}

- (void)addObject:(id)object
{
	if([_removed containsObject:object])
		[_removed removeObject:object];
	else
		[_added addObject:object];
}

- (BOOL)containsObject:(id)object
{
	if([_added containsObject:object])
		return YES;
	else if([_removed containsObject:object])
		return NO;
	else
		return [_initial containsObject:object];
}

- (void)removeObject:(id)object
{
	if([_added containsObject:object])
		[_added removeObject:object];
	else
		[_removed addObject:object];
}

- (void)removeAllObjects
{
	[_added removeAllObjects];
	[_removed setSet:_initial];
}

- (NSUInteger)count
{
	return _initial.count - _removed.count + _added.count;
}

- (NSSet *)initialObjects
{
	return _initial;
}

- (NSSet *)addedObjects
{
	return [_added copy];
}

- (NSSet *)removedObjects
{
	return [_removed copy];
}

- (NSSet *)mergedObjects
{
	NSMutableSet *merged = [_initial mutableCopy];
	[merged minusSet:_removed];
	[merged unionSet:_added];
	return [merged copy];
}

- (BOOL)hasChanges
{
	return _added.count != 0 || _removed.count != 0;
}

- (void)mergeChanges
{
	_initial = [self mergedObjects];
	[_added removeAllObjects];
	[_removed removeAllObjects];
}

- (void)revertChanges
{
	[_added removeAllObjects];
	[_removed removeAllObjects];
}

- (void)reset
{
	_initial = [NSSet set];
	[_added removeAllObjects];
	[_removed removeAllObjects];
}

@end
