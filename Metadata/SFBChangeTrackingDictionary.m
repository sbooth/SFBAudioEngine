/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBChangeTrackingDictionary.h"

static NSString * const SFBChangeTrackingDictionaryNullSentinel = @"Null";

@interface SFBChangeTrackingDictionary ()
{
@private
	NSDictionary *_initial;
	NSMutableDictionary *_changes;
}
@end

@implementation SFBChangeTrackingDictionary

- (instancetype)init
{
	if((self = [super init])) {
		_initial = [NSDictionary dictionary];
		_changes = [NSMutableDictionary dictionary];
	}
	return self;
}

- (instancetype)initWithInitialValues:(NSDictionary *)initialValues
{
	if((self = [super init])) {
		_initial = [initialValues copy];
		_changes = [NSMutableDictionary dictionary];
	}
	return self;
}

- (id)objectForKey:(id)key
{
	id object = nil;
	if((object = _changes[key])) {
		return object == SFBChangeTrackingDictionaryNullSentinel ? nil : object;
	}
	else
		return _initial[key];
}

- (void)setObject:(id)object forKey:(id)key
{
	if(object) {
		id value = _initial[key];
		if(_changes[key])
			_changes[key] = [value isEqual:object] ? nil : object;
		else if(!value || ![value isEqual:object])
			_changes[key] = object;
	}
	else
		_changes[key] = _initial[key] ? SFBChangeTrackingDictionaryNullSentinel : nil;
}

- (void)removeObjectForKey:(id)key
{
	[self setObject:nil forKey:key];
}

- (void)removeAllObjects
{
	[_changes removeAllObjects];
	for(id key in _initial)
		_changes[key] = SFBChangeTrackingDictionaryNullSentinel;
}

- (NSUInteger)count
{
	NSUInteger count = _initial.count;
	for(id key in _changes)
		_changes[key] == SFBChangeTrackingDictionaryNullSentinel ? --count : ++count;
	return count;
}

- (NSDictionary *)mergedValues
{
	NSMutableDictionary *merged = [_initial mutableCopy];
	for(id key in _changes) {
		id value = _changes[key];
		merged[key] = value == SFBChangeTrackingDictionaryNullSentinel ? nil : value;
	}
	return [merged copy];
}

- (BOOL)hasChanges
{
	return _changes.count != 0;
}

- (BOOL)hasChangesForKey:(id)key
{
	return _changes[key] != nil;
}

- (void)mergeChanges
{
	_initial = [self mergedValues];
	[_changes removeAllObjects];
}

- (void)revertChanges
{
	[_changes removeAllObjects];
}

- (void)addEntriesFromDictionary:(NSDictionary *)dictionary
{
	for(id key in dictionary)
		[self setObject:[dictionary objectForKey:key] forKey:key];
}

- (void)reset
{
	_initial = [NSDictionary dictionary];
	[_changes removeAllObjects];
}

@end
