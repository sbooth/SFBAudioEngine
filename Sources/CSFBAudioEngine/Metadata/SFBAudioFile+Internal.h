//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBAudioFile.h"

#import "SFBTernaryTruthValue.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioFileLog;

@interface SFBAudioFile ()
/// Returns the audio file format name
@property (class, nonatomic, readonly) SFBAudioFileFormatName formatName;
/// The file's audio properties
@property (nonatomic) SFBAudioProperties *properties;

/// Tests whether a file handle contains data in a supported format
/// - parameter fileHandle: The file handle containing the data to test
/// - parameter formatIsSupported: On return indicates whether the data in `fileHandle` is a supported format
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the test was successfully performed, `NO` otherwise
+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error;

@end

#pragma mark - Subclass Registration

@interface SFBAudioFile (SFBAudioFileSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
