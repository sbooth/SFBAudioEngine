/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <objc/runtime.h>
#import <os/log.h>

#import "SFBAudioFile.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioFileLog;

@interface SFBAudioFile ()
@property (nonatomic) SFBAudioProperties *properties;
@end

@interface SFBAudioFileSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@interface SFBAudioFile (SFBAudioFileSubclassLookup)
+ (nullable Class)subclassForURL:(NSURL *)url;
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
@end

NS_ASSUME_NONNULL_END
