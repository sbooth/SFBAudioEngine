/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <objc/runtime.h>

#import "SFBAudioMetadata.h"
#import "SFBChangeTrackingDictionary.h"
#import "SFBChangeTrackingSet.h"

NS_ASSUME_NONNULL_BEGIN

// Subclass registration support
@interface SFBAudioMetadataSubclassInfo: NSObject
@property (nonatomic) Class subclass;
@property (nonatomic) int priority;
@end


@interface SFBAudioMetadata (SubclassRegistration)
+ (void)registerSubclass:(Class)subclass;
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
+ (NSArray<SFBAudioMetadataSubclassInfo *> *)registeredSubclasses;
@end


@interface SFBAudioMetadata ()
{
@protected
	SFBChangeTrackingDictionary *_metadata;
	SFBChangeTrackingSet<SFBAttachedPicture *> *_pictures;
}
@end


// Subclasses must implement the following methods
@interface SFBAudioMetadata (RequiredSubclassMethods)
- (BOOL)_readMetadata:(NSError * _Nullable *)error;
- (BOOL)_writeMetadata:(NSError * _Nullable *)error;
@end


// Utility category
@interface NSError (SFBAudioMetadataMethods)
+ (instancetype)sfb_audioMetadataErrorWithCode:(NSInteger)code descriptionFormatStringForURL:(NSString *)descriptionFormatStringForURL url:(NSURL *)url failureReason:(NSString *)failureReason recoverySuggestion:(NSString *)recoverySuggestion;
@end

NS_ASSUME_NONNULL_END
