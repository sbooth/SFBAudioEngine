/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadataSubclassInfo
@end


@implementation SFBAudioMetadata (SubclassRegistration)

static NSMutableArray *_registeredSubclasses = nil;

+ (void) registerSubclass:(Class)subclass
{
	[self registerSubclass:subclass priority:0];
}

+ (void) registerSubclass:(Class)subclass priority:(int)priority
{
	NSParameterAssert([subclass isKindOfClass:[SFBAudioMetadata class]]);

	if(_registeredSubclasses == nil) {
		_registeredSubclasses = [NSMutableArray array];
	}

	SFBAudioMetadataSubclassInfo *subclassInfo = [[SFBAudioMetadataSubclassInfo alloc] init];
	subclassInfo.subclass = subclass;
	subclassInfo.priority = priority;

	[_registeredSubclasses addObject:subclassInfo];
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
		return ((SFBAudioMetadataSubclassInfo *)obj1).priority < ((SFBAudioMetadataSubclassInfo *)obj2).priority;
	}];
}

+ (NSArray *)registeredSubclasses
{
	return _registeredSubclasses;
}

@end


@implementation SFBAudioMetadata (RequiredSubclassMethods)

- (BOOL)_readMetadata:(NSError **)error
{
#pragma unused(error)
	NSAssert(0, @"SFBAudioMetadata subclasses are required to implement _readMetadata");
	return NO;
}

- (BOOL)_writeMetadata:(NSError **)error
{
	#pragma unused(error)
	NSAssert(0, @"SFBAudioMetadata subclasses are required to implement _writeMetadata");
	return NO;
}

@end


@implementation NSError (SFBAudioMetadataMethods)

+ (instancetype)sfb_audioMetadataErrorWithCode:(NSInteger)code descriptionFormatStringForURL:(NSString *)descriptionFormatStringForURL url:(NSURL *)url failureReason:(NSString *)failureReason recoverySuggestion:(NSString *)recoverySuggestion
{
	NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];

	NSString *displayName = nil;
	if([url getResourceValue:&displayName forKey:NSURLLocalizedNameKey error:nil])
		userInfo[NSLocalizedDescriptionKey] = [NSString stringWithFormat:descriptionFormatStringForURL, displayName];
	else
		userInfo[NSLocalizedDescriptionKey] = [NSString stringWithFormat:descriptionFormatStringForURL, url.lastPathComponent];

	userInfo[NSLocalizedFailureReasonErrorKey] = failureReason;
	userInfo[NSLocalizedRecoverySuggestionErrorKey] = recoverySuggestion;

	return [NSError errorWithDomain:SFBAudioMetadataErrorDomain code:code userInfo:userInfo];
}

@end
