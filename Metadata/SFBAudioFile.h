/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBAudioProperties.h>
#import <SFBAudioEngine/SFBAudioMetadata.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio file containing properties (like channel count and sample rate) and metadata (like artist name and album title)
NS_SWIFT_NAME(AudioFile) @interface SFBAudioFile : NSObject

/// Returns an array containing the supported file extensions
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/// Returns an array containing the supported MIME types
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/// Tests whether a file extension is supported
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/// Tests whether a MIME type is supported
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

/// Returns an initialized \c SFBAudioFile object for the specified URL populated with audio properties and metadata or \c nil on failure
/// @param url The URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return An \c SFBAudioFile object or \c nil on failure
+ (nullable instancetype)audioFileWithURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(init(readingPropertiesAndMetadataFrom:));

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioFile object for the specified URL
/// @note Does not read audio properties or metadata
/// @param url The desired URL
- (nullable instancetype)initWithURL:(NSURL *)url NS_DESIGNATED_INITIALIZER;

/// The URL of the file
@property (nonatomic, readonly) NSURL *url;

/// The file's audio properties
@property (nonatomic, readonly) SFBAudioProperties *properties;

/// The file's audio metadata
@property (nonatomic) SFBAudioMetadata *metadata;

/// Reads audio properties and metadata
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful, \c NO otherwise
- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error NS_SWIFT_NAME(readPropertiesAndMetadata());

/// Writes metadata
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful, \c NO otherwise
- (BOOL)writeMetadataReturningError:(NSError **)error NS_SWIFT_NAME(writeMetadata());

@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioFile and subclasses
extern NSErrorDomain const SFBAudioFileErrorDomain NS_SWIFT_NAME(AudioFile.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioFile
typedef NS_ERROR_ENUM(SFBAudioFileErrorDomain, SFBAudioFileErrorCode) {
	/// File format not recognized
	SFBAudioFileErrorCodeFileFormatNotRecognized		= 0,
	/// File format not supported
	SFBAudioFileErrorCodeFileFormatNotSupported			= 1,
	/// Input/output error
	SFBAudioFileErrorCodeInputOutput					= 2
} NS_SWIFT_NAME(AudioFile.ErrorCode);

NS_ASSUME_NONNULL_END
