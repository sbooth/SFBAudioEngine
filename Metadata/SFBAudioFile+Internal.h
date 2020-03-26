/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <objc/runtime.h>

#import "SFBAudioFile.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioFile ()
@property (nonatomic) NSURL *url;
@property (nonatomic) SFBAudioProperties *properties;
@end

@protocol SFBAudioFileInputOutputHandling
+ (NSSet<NSString *> *)supportedPathExtensions;
+ (NSSet<NSString *> *)supportedMIMETypes;
- (BOOL)readAudioPropertiesAndMetadataFromURL:(NSURL *)url toAudioFile:(SFBAudioFile *)audioFile error:(NSError * _Nullable *)error;
- (BOOL)writeAudioMetadata:(SFBAudioMetadata *)metadata toURL:(NSURL *)url error:(NSError * _Nullable *)error;
@end

@interface SFBAudioFileInputOutputHandlerInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@interface SFBAudioFile (SFBAudioFileInputOutputHandling)
+ (void)registerInputOutputHandler:(Class)handler;
+ (void)registerInputOutputHandler:(Class)handler priority:(int)priority;

@property (nonatomic, class, readonly) NSArray<id<SFBAudioFileInputOutputHandling>> *registeredInputOutputHandlers;

+ (nullable id<SFBAudioFileInputOutputHandling>)inputOutputHandlerForURL:(NSURL *)url;
+ (nullable id<SFBAudioFileInputOutputHandling>)inputOutputHandlerForPathExtension:(NSString *)extension;
+ (nullable id<SFBAudioFileInputOutputHandling>)inputOutputHandlerForMIMEType:(NSString *)mimeType;
@end

NS_ASSUME_NONNULL_END
