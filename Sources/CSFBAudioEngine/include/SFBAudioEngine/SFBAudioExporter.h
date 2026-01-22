//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

@protocol SFBPCMDecoding;

NS_ASSUME_NONNULL_BEGIN

/// A class that exports audio using `AVAudioFile`
NS_SWIFT_NAME(AudioExporter)
@interface SFBAudioExporter : NSObject

/// Exports audio to the specified URL
/// - note: The file type to create is inferred from the file extension of `targetURL`
/// - parameter sourceURL: The URL to export
/// - parameter targetURL: The destination URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
+ (BOOL)exportFromURL:(NSURL *)sourceURL
                toURL:(NSURL *)targetURL
                error:(NSError **)error NS_SWIFT_NAME(AudioExporter.export(_:to:));

/// Exports audio to the specified URL
/// - note: The file type to create is inferred from the file extension of `targetURL`
/// - parameter decoder: The decoder to export
/// - parameter targetURL: The destination URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` otherwise
+ (BOOL)exportFromDecoder:(id<SFBPCMDecoding>)decoder
                    toURL:(NSURL *)targetURL
                    error:(NSError **)error NS_SWIFT_NAME(AudioExporter.export(_:to:));

@end

/// The `NSErrorDomain` used by `SFBAudioExporter`
extern NSErrorDomain const SFBAudioExporterErrorDomain NS_SWIFT_NAME(AudioExporter.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioExporter`
typedef NS_ERROR_ENUM(SFBAudioExporterErrorDomain, SFBAudioExporterErrorCode){
    /// File format not supported
    SFBAudioExporterErrorCodeFileFormatNotSupported = 0,
} NS_SWIFT_NAME(AudioExporter.Error);

NS_ASSUME_NONNULL_END
