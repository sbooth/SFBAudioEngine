/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBPCMDecoding.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief A decoder providing audio as PCM */
NS_SWIFT_NAME(AudioDecoder) @interface SFBAudioDecoder : NSObject <SFBPCMDecoding>

#pragma mark - File Format Support

/*! @brief Returns a set containing the supported path extensions */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/*!@brief Returns a set containing the supported MIME types */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/*! @brief Tests whether a file extension is supported */
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/*! @brief Tests whether a MIME type is supported */
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

#pragma mark - Represented Object

/*! @brief An arbitrary object to associate with the decoder */
@property (nonatomic, nullable) id representedObject;

@end

#pragma mark - Subclass Registration

@interface SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)
/*! @brief Register a subclass with the default priority (\c 0) */
+ (void)registerSubclass:(Class)subclass;

/*! @brief Register a subclass with the specified priority*/
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

#pragma mark - Error Information

/*! @brief The \c NSErrorDomain used by \c SFBAudioDecoder and subclasses */
extern NSErrorDomain const SFBAudioDecoderErrorDomain NS_SWIFT_NAME(AudioDecoderErrorDomain);

/*! @brief Possible \c NSError  error codes used by \c SFBAudioDecoder */
typedef NS_ERROR_ENUM(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCode) {
	SFBAudioDecoderErrorCodeFileNotFound	= 0,		/*!< File not found */
	SFBAudioDecoderErrorCodeInputOutput		= 1			/*!< Input/output error */
};

NS_ASSUME_NONNULL_END
