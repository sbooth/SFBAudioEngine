/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

#import "SFBAudioProperties.h"
#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief The \c NSErrorDomain used by \c SFBAudioFile and subclasses */
extern NSErrorDomain const SFBAudioFileErrorDomain;

/*! @brief Possible \c NSError  error codes used by \c SFBAudioFile */
typedef NS_ERROR_ENUM(SFBAudioFileErrorDomain, SFBAudioFileErrorCode) {
	SFBAudioFileErrorCodeFileFormatNotRecognized		= 0,	/*!< File format not recognized */
	SFBAudioFileErrorCodeFileFormatNotSupported			= 1,	/*!< File format not supported */
	SFBAudioFileErrorCodeInputOutput					= 2		/*!< Input/output error */
};

/*! @brief An audio file  */
@interface SFBAudioFile : NSObject

/*! @brief Returns an array containing the supported file extensions */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/*!@brief Returns  an array containing the supported MIME types */
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/*! @brief Tests whether a file extension is supported */
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/*! @brief Tests whether a MIME type is supported */
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

/*!
 * @brief Returns an \c SFBAudioFile  for the specified URL populated with audio properties and metadata or \c nil on failure
 * @param url The URL
 * @param error An optional pointer to an \c NSError to receive error information
 * @return An \c SFBAudioFile object or \c nil on failure
 */
+ (nullable instancetype)audioFileWithURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(init(readingPropertiesAndMetadataFrom:));

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*!
 * @brief Returns an initialized  \c SFBAudioFile object for the specified URL
 * @discussion Does not read audio properties or metadata
 * @param url The desired URL
 */
- (nullable instancetype)initWithURL:(NSURL *)url NS_DESIGNATED_INITIALIZER;

/*! @brief The URL of the file */
@property (nonatomic, readonly) NSURL *url;

/*! @brief The file's audio properties */
@property (nonatomic, readonly) SFBAudioProperties *properties;

/*! @brief The file's audio metadata */
@property (nonatomic) SFBAudioMetadata *metadata;

/*!
 * @brief Reads audio properties and metadata
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES if successful, \c NO otherwise
 */
- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error NS_SWIFT_NAME(readPropertiesAndMetadata());

/*!
 * @brief Writes metadata
 * @param error An optional pointer to an \c NSError to receive error information
 * @return \c YES if successful, \c NO otherwise
 */
- (BOOL)writeMetadataReturningError:(NSError **)error NS_SWIFT_NAME(writeMetadata());

@end

@interface SFBAudioFile (SFBAudioFileSubclassRegistration)
+ (void)registerSubclass:(Class)subclass;
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

NS_ASSUME_NONNULL_END
