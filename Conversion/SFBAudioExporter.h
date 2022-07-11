//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

@protocol SFBPCMDecoding;

NS_ASSUME_NONNULL_BEGIN

/// A class that exports audio using \c AVAudioFile
NS_SWIFT_NAME(AudioExporter) @interface SFBAudioExporter : NSObject

/// Exports audio to the specified URL
/// @note The file type to create is inferred from the file extension of \c targetURL
/// @param sourceURL The URL to export
/// @param targetURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)exportFromURL:(NSURL *)sourceURL toURL:(NSURL *)targetURL error:(NSError **)error NS_SWIFT_NAME(AudioExporter.export(_:to:));

/// Exports audio to the specified URL
/// @note The file type to create is inferred from the file extension of \c targetURL
/// @param decoder The decoder to export
/// @param targetURL The destination URL
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES on success, \c NO otherwise
+ (BOOL)exportFromDecoder:(id <SFBPCMDecoding>)decoder toURL:(NSURL *)targetURL error:(NSError **)error NS_SWIFT_NAME(AudioExporter.export(_:to:));

@end

/// The \c NSErrorDomain used by \c SFBAudioExporter
extern NSErrorDomain const SFBAudioExporterErrorDomain NS_SWIFT_NAME(AudioExporter.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioExporter
typedef NS_ERROR_ENUM(SFBAudioExporterErrorDomain, SFBAudioExporterErrorCode) {
	/// File format not supported
	SFBAudioExporterErrorCodeFileFormatNotSupported				= 0
} NS_SWIFT_NAME(AudioExporter.ErrorCode);

NS_ASSUME_NONNULL_END
