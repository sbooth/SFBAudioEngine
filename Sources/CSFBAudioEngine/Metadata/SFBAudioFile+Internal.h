//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioFile.h"
#import "SFBTernaryTruthValue.h"

#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioFileLog;

@interface SFBAudioFile ()
/// Returns the audio file format name
@property(class, nonatomic, readonly) SFBAudioFileFormatName formatName;
/// The file's audio properties
@property(nonatomic) SFBAudioProperties *properties;

/// Tests whether a file handle contains data in a supported format
/// - parameter fileHandle: The file handle containing the data to test
/// - parameter formatIsSupported: On return indicates whether the data in `fileHandle` is a supported format
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if the test was successfully performed, `NO` otherwise
+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
        formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                    error:(NSError **)error;

/// Returns an invalid format error with a description similar to "The file is not a valid XXX file"
/// - parameter formatName: The localized name of the audio format
/// - returns: An error in `SFBAudioFileErrorDomain` with code `SFBAudioFileErrorCodeInvalidFormat`
- (NSError *)genericInvalidFormatError:(NSString *)formatName;
/// Returns an input/output error with a description similar to "The file could not be opened for reading"
/// - returns: An error in `SFBAudioFileErrorDomain` with code `SFBAudioFileErrorCodeInputOutput`
- (NSError *)genericOpenForReadingError;
/// Returns an input/output error with a description similar to "The file could not be opened for writing"
/// - returns: An error in `SFBAudioFileErrorDomain` with code `SFBAudioFileErrorCodeInputOutput`
- (NSError *)genericOpenForWritingError;
/// Returns an input/output error with a description similar to "The file could not be saved"
/// - returns: An error in `SFBAudioFileErrorDomain` with code `SFBAudioFileErrorCodeInputOutput`
- (NSError *)genericSaveError;
/// Returns an input/output error with a description similar to "The file could not be saved"
/// - parameter recoverySuggestion: A localized error recovery suggestion
/// - returns: An error in `SFBAudioFileErrorDomain` with code `SFBAudioFileErrorCodeInputOutput`
- (NSError *)saveErrorWithRecoverySuggestion:(NSString *)recoverySuggestion;
@end

#pragma mark - Subclass Registration

@interface SFBAudioFile (SFBAudioFileSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
