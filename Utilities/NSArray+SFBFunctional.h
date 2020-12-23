/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSArray<ObjectType> (SFBFunctional)
/// Returns an array containing the results of applying \c block to each element in \c self
- (NSArray *)mappedArrayUsingBlock:(id (^)(ObjectType obj))block;
/// Returns a copy of \c self including only elements for which \c block returns \c YES
- (NSArray<ObjectType> *)filteredArrayUsingBlock:(BOOL (^)(ObjectType obj))block;
@end

NS_ASSUME_NONNULL_END
