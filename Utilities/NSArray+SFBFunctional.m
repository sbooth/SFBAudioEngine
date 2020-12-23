/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "NSArray+SFBFunctional.h"

@implementation NSArray (SFBFunctional)

- (NSArray *)mappedArrayUsingBlock:(id (^)(id))block
{
	NSParameterAssert(block != NULL);
	
	NSMutableArray *result = [NSMutableArray arrayWithCapacity:self.count];
	for(id obj in self) {
		id newobj = block(obj);
		[result addObject:(newobj ?: [NSNull null])];
	}
	return result;
}

- (NSArray *)filteredArrayUsingBlock:(BOOL (^)(id))block
{
	NSParameterAssert(block != NULL);

	return [self objectsAtIndexes:[self indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return block(obj);
	}]];
}

@end
