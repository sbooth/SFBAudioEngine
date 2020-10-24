/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

@class SFBAudioDecoder;

NS_ASSUME_NONNULL_BEGIN

/// A class that exports audio using \c AVAudioFile
NS_SWIFT_NAME(AudioExporter) @interface SFBAudioExporter : NSObject

/// Exports audio to the specified URL
/// @note The file type to create is inferred from the file extension of \c targetURL
/// @param sourceURL The URL to export
/// @param targetURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)exportURL:(NSURL *)sourceURL toURL:(NSURL *)targetURL error:(NSError **)error;

/// Exports audio to the specified URL
/// @note The file type to create is inferred from the file extension of \c targetURL
/// @param decoder The decoder to export
/// @param targetURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)exportDecoder:(SFBAudioDecoder *)decoder toURL:(NSURL *)targetURL error:(NSError **)error;

@end

/// The \c NSErrorDomain used by \c SFBAudioExporter
extern NSErrorDomain const SFBAudioExporterErrorDomain NS_SWIFT_NAME(AudioExporter.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioExporter
typedef NS_ERROR_ENUM(SFBAudioExporterErrorDomain, SFBAudioExporterErrorCode) {
	/// File format not supported
	SFBAudioExporterErrorCodeFileFormatNotSupported				= 0
} NS_SWIFT_NAME(AudioExporter.ErrorCode);

NS_ASSUME_NONNULL_END
