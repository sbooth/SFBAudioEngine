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

@interface SFBAudioMetadata ()
{
@private
	SFBChangeTrackingDictionary *_metadata;
	SFBChangeTrackingSet<SFBAttachedPicture *> *_pictures;
}
@end


@interface SFBAudioMetadata (Internal)
@property (nonatomic, nullable) NSString *formatName;
@property (nonatomic, nullable) NSNumber *totalFrames;
@property (nonatomic, nullable) NSNumber *channelsPerFrame;
@property (nonatomic, nullable) NSNumber *bitsPerChannel;
@property (nonatomic, nullable) NSNumber *sampleRate;
@property (nonatomic, nullable) NSNumber *duration;
@property (nonatomic, nullable) NSNumber *bitrate;
@end


// Subclass registration support
@interface SFBAudioMetadataSubclassInfo : NSObject
@property (nonatomic) Class subclass;
@property (nonatomic) int priority;
@end


@interface SFBAudioMetadata (SubclassRegistration)
+ (void)registerSubclass:(Class)subclass;
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
+ (NSArray<SFBAudioMetadataSubclassInfo *> *)registeredSubclasses;
@end


// Subclasses must implement the following methods
@interface SFBAudioMetadata (RequiredSubclassMethods)
- (BOOL)_readMetadata:(NSError * _Nullable *)error;
- (BOOL)_writeMetadata:(NSError * _Nullable *)error;
@end

NS_ASSUME_NONNULL_END
