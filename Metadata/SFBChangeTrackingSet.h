/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*! @brief A set-like object tracking changes from initial values */
@interface SFBChangeTrackingSet<__covariant ObjectType> : NSObject
- (instancetype)initWithInitialObjects:(NSArray<ObjectType> *)initialObjects;

- (void)addObject:(ObjectType)object;
- (BOOL)containsObject:(ObjectType)object;

- (void)removeObject:(ObjectType)object;
- (void)removeAllObjects;

@property (nonatomic, readonly) NSUInteger count;

@property (nonatomic, readonly) NSSet<ObjectType> *initialObjects;
@property (nonatomic, readonly) NSSet<ObjectType> *addedObjects;
@property (nonatomic, readonly) NSSet<ObjectType> *removedObjects;
@property (nonatomic, readonly) NSSet<ObjectType> *mergedObjects;

@property (nonatomic, readonly) BOOL hasChanges;

- (void)mergeChanges;
- (void)revertChanges;

- (void)reset;
@end

NS_ASSUME_NONNULL_END
