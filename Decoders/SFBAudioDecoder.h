/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

#import "SFBAudioBufferList.h"
#import "SFBAudioFormat.h"
#import "SFBInputSource.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief The \c NSErrorDomain used by \c SFBAudioDecoder and subclasses */
extern NSErrorDomain const SFBAudioDecoderErrorDomain;

/*! @brief Possible \c NSError  error codes used by \c SFBAudioDecoder */
typedef NS_ERROR_ENUM(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCode) {
	SFBAudioDecoderErrorCodeFileNotFound	= 0,		/*!< File not found */
	SFBAudioDecoderErrorCodeInputOutput		= 1			/*!< Input/output error */
};

/*! @brief A decoder's represented object, typically a C struct. If used from the real time thread it must not contain calls to Swift, Objective-C, or CF objects. */
typedef void *SFBAudioDecoderRepresentedObject;

/*! @brief A block freeing any resources associated with a decoder's represented object*/
typedef void(^SFBAudioDecoderRepresentedObjectCleanupBlock)(SFBAudioDecoderRepresentedObject);

@interface SFBAudioDecoder : NSObject
{
@public
	SFBAudioDecoderRepresentedObject _representedObject; /*!< This is safe to access directly using \c -> in IOProcs */
}

/*! @brief Returns a set containing the supported path extensions */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/*!@brief Returns  a set containing the supported MIME types */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/*! @brief Tests whether a file extension is supported */
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/*! @brief Tests whether a MIME type is supported */
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url;
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error;

- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error;
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) SFBInputSource *inputSource;

@property (nonatomic, readonly) SFBAudioFormat *sourceFormat;
@property (nonatomic, readonly) NSString *sourceFormatDescription;

@property (nonatomic, readonly) SFBAudioFormat *processingFormat;
@property (nonatomic, readonly) NSString *processingFormatDescription;

/*!
 * @brief Opens the decoder for reading
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)openReturningError:(NSError **)error NS_SWIFT_NAME(open()) NS_REQUIRES_SUPER;

/*!
 * Closes the decoder
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)closeReturningError:(NSError **)error NS_SWIFT_NAME(close()) NS_REQUIRES_SUPER;

/*! @brief Returns \c YES if the decoder is open */
@property (nonatomic, readonly) BOOL isOpen;

- (BOOL)decodeAudio:(SFBAudioBufferList *)bufferList frameCount:(NSInteger)frameCount framesRead:(NSInteger *)framesRead error:(NSError **)error;

@property (nonatomic, readonly) NSInteger currentFrame;
@property (nonatomic, readonly) NSInteger totalFrames;
@property (nonatomic, readonly) NSInteger framesRemaining;

@property (nonatomic, readonly) BOOL supportsSeeking;

- (BOOL)seekToFrame:(NSInteger)frame error:(NSError **)error;

@property (nonatomic, nullable) SFBAudioDecoderRepresentedObject representedObject;
@property (nonatomic, nullable) SFBAudioDecoderRepresentedObjectCleanupBlock representedObjectCleanupBlock;

@end

@interface SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)
+ (void)registerSubclass:(Class)subclass;
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
