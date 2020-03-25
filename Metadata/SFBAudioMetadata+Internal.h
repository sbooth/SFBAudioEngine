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


@interface SFBAudioMetadata (SFBAudioMetadataInternal)
@property (nonatomic, nullable) NSString *formatName;
@property (nonatomic, nullable) NSNumber *totalFrames;
@property (nonatomic, nullable) NSNumber *channelsPerFrame;
@property (nonatomic, nullable) NSNumber *bitsPerChannel;
@property (nonatomic, nullable) NSNumber *sampleRate;
@property (nonatomic, nullable) NSNumber *duration;
@property (nonatomic, nullable) NSNumber *bitrate;
@end


@protocol SFBAudioMetadataInputOutputHandling
+ (NSSet<NSString *> *)supportedPathExtensions;
+ (NSSet<NSString *> *)supportedMIMETypes;
- (nullable SFBAudioMetadata *)readAudioMetadataFromURL:(NSURL *)url error:(NSError * _Nullable *)error;
- (BOOL)writeAudioMetadata:(SFBAudioMetadata *)metadata toURL:(NSURL *)url error:(NSError * _Nullable *)error;
@end


@interface SFBAudioMetadataInputOutputHandlerInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@interface SFBAudioMetadata (SFBAudioMetadataInputOutputHandling)
+ (void)registerInputOutputHandler:(Class)reader;
+ (void)registerInputOutputHandler:(Class)reader priority:(int)priority;

@property (nonatomic, class, readonly) NSArray<id<SFBAudioMetadataInputOutputHandling>> *registeredInputOutputHandlers;

+ (nullable id<SFBAudioMetadataInputOutputHandling>)inputOutputHandlerForURL:(NSURL *)url;
+ (nullable id<SFBAudioMetadataInputOutputHandling>)inputOutputHandlerForPathExtension:(NSString *)extension;
+ (nullable id<SFBAudioMetadataInputOutputHandling>)inputOutputHandlerForMIMEType:(NSString *)mimeType;
@end

NS_ASSUME_NONNULL_END
