//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <objc/runtime.h>
#import <os/log.h>

#import "SFBAudioFile.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioFileLog;

@interface SFBAudioFile ()
/// Returns the audio file format name
@property (class, nonatomic, readonly) SFBAudioFileFormatName formatName;
@property (nonatomic) SFBAudioProperties *properties;
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBAudioFile (SFBAudioFileSubclassRegistration)
/// Register a subclass with the default priority (\c 0)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

@interface SFBAudioFile (SFBAudioFileSubclassLookup)
/// Returns the appropriate \c SFBAudioFile subclass for \c url
+ (nullable Class)subclassForURL:(NSURL *)url;
/// Returns the appropriate \c SFBAudioFile subclass for paths with \c extension
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
/// Returns the appropriate \c SFBAudioFile subclass for data of \c mimeType
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
/// Returns the appropriate \c SFBAudioFile subclass corresponding to \c formatName
+ (nullable Class)subclassForFormatName:(SFBAudioFileFormatName)formatName;
@end

NS_ASSUME_NONNULL_END
