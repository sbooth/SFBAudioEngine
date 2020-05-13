/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBDSDDecoding.h"

NS_ASSUME_NONNULL_BEGIN

/// A decoder providing audio as DSD
NS_SWIFT_NAME(DSDDecoder) @interface SFBDSDDecoder : NSObject <SFBDSDDecoding>

#pragma mark - File Format Support

/// Returns a set containing the supported path extensions
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/*!@brief Returns  a set containing the supported MIME types */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/// Tests whether a file extension is supported
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/// Tests whether a MIME type is supported
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

#pragma mark - Creation

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource NS_SWIFT_UNAVAILABLE("Use -initWithInputSource:error: instead");
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;
@end

#pragma mark - Subclass Registration

@interface SFBDSDDecoder (SFBDSDDecoderSubclassRegistration)
/// Register a subclass with the default priority (\c 0)
+ (void)registerSubclass:(Class)subclass;

/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBDSDDecoder and subclasses
extern NSErrorDomain const SFBDSDDecoderErrorDomain NS_SWIFT_NAME(DSDDecoder.ErrorDomain);

/// Possible \c NSError  error codes used by \c SFBDSDDecoder
typedef NS_ERROR_ENUM(SFBDSDDecoderErrorDomain, SFBDSDDecoderErrorCode) {
	/// File not found
	SFBDSDDecoderErrorCodeFileNotFound		= 0,
	/// Input/output error
	SFBDSDDecoderErrorCodeInputOutput		= 1
} NS_SWIFT_NAME(DSDDecoder.ErrorCode);

NS_ASSUME_NONNULL_END

