/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*! @brief A dictionary-like object tracking changes from initial values */
@interface SFBChangeTrackingDictionary<KeyType, ObjectType> : NSObject
- (instancetype)initWithInitialValues:(NSDictionary<KeyType, ObjectType> *)initialValues;

- (nullable ObjectType) objectForKey:(KeyType)key;
- (void)setObject:(nullable ObjectType)object forKey:(KeyType)key;

- (void)removeObjectForKey:(KeyType)key;
- (void)removeAllObjects;

- (NSUInteger)count;

- (NSDictionary<KeyType, ObjectType> *) mergedValues;

- (BOOL)hasChanges;
- (BOOL)hasChangesForKey:(KeyType)key;

- (void)mergeChanges;
- (void)revertChanges;

- (void)addEntriesFromDictionary:(NSDictionary<KeyType, ObjectType> *)dictionary;

- (void)reset;
@end

NS_ASSUME_NONNULL_END
