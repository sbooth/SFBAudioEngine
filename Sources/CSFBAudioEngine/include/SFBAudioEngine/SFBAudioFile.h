//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBAudioMetadata.h>
#import <SFBAudioEngine/SFBAudioProperties.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Constant type for file format names
typedef NSString *SFBAudioFileFormatName NS_TYPED_ENUM NS_SWIFT_NAME(AudioFile.FormatName);

/// AIFF
extern SFBAudioFileFormatName const SFBAudioFileFormatNameAIFF;
/// DSDIFF
extern SFBAudioFileFormatName const SFBAudioFileFormatNameDSDIFF;
/// DSF
extern SFBAudioFileFormatName const SFBAudioFileFormatNameDSF;
/// Extended Module
extern SFBAudioFileFormatName const SFBAudioFileFormatNameExtendedModule;
/// FLAC
extern SFBAudioFileFormatName const SFBAudioFileFormatNameFLAC;
/// Impulse Tracker Module
extern SFBAudioFileFormatName const SFBAudioFileFormatNameImpulseTrackerModule;
/// Monkey's Audio
extern SFBAudioFileFormatName const SFBAudioFileFormatNameMonkeysAudio;
/// MP3
extern SFBAudioFileFormatName const SFBAudioFileFormatNameMP3;
/// MP4
extern SFBAudioFileFormatName const SFBAudioFileFormatNameMP4;
/// Musepack
extern SFBAudioFileFormatName const SFBAudioFileFormatNameMusepack;
/// Ogg FLAC
extern SFBAudioFileFormatName const SFBAudioFileFormatNameOggFLAC;
/// Ogg Opus
extern SFBAudioFileFormatName const SFBAudioFileFormatNameOggOpus;
/// Ogg Speex
extern SFBAudioFileFormatName const SFBAudioFileFormatNameOggSpeex;
/// Ogg Vorbis
extern SFBAudioFileFormatName const SFBAudioFileFormatNameOggVorbis;
/// ProTracker Module
extern SFBAudioFileFormatName const SFBAudioFileFormatNameProTrackerModule;
/// Scream Tracker 3 Module
extern SFBAudioFileFormatName const SFBAudioFileFormatNameScreamTracker3Module;
/// Shorten
extern SFBAudioFileFormatName const SFBAudioFileFormatNameShorten;
/// True Audio
extern SFBAudioFileFormatName const SFBAudioFileFormatNameTrueAudio;
/// WAVE
extern SFBAudioFileFormatName const SFBAudioFileFormatNameWAVE;
/// WavPack
extern SFBAudioFileFormatName const SFBAudioFileFormatNameWavPack;

/// An audio file containing properties (like channel count and sample rate) and metadata (like artist name and album
/// title)
NS_SWIFT_NAME(AudioFile)
@interface SFBAudioFile : NSObject

/// Returns an array containing the supported file extensions
@property(class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/// Returns an array containing the supported MIME types
@property(class, nonatomic, readonly) NSSet<NSString *> *supportedMIMETypes;

/// Tests whether a file extension is supported
+ (BOOL)handlesPathsWithExtension:(NSString *)extension;

/// Tests whether a MIME type is supported
+ (BOOL)handlesMIMEType:(NSString *)mimeType;

/// Reads metadata from `sourceURL` and writes it to `destinationURL`
/// - parameter sourceURL: The source URL
/// - parameter destinationURL: The destination URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` on success, `NO` if metadata couldn't be read or written
+ (BOOL)copyMetadataFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error;

/// Returns an initialized `SFBAudioFile` object for the specified URL populated with audio properties and metadata or
/// `nil` on failure
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An `SFBAudioFile` object or `nil` on failure
+ (nullable instancetype)audioFileWithURL:(NSURL *)url
                                    error:(NSError **)error NS_SWIFT_NAME(init(readingPropertiesAndMetadataFrom:));

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBAudioFile` object for the given URL or `nil` on failure
/// - note: Does not read audio properties or metadata
/// - parameter url: The URL
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized `SFBAudioFile` object for the given URL or `nil` on failure
/// - note: Does not read audio properties or metadata
/// - parameter url: The URL
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioFile` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized `SFBAudioFile` object for the given URL or `nil` on failure
/// - note: Does not read audio properties or metadata
/// - parameter url: The URL
/// - parameter detectContentType: Whether to attempt to determine the content type of `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioFile` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url detectContentType:(BOOL)detectContentType error:(NSError **)error;
/// Returns an initialized `SFBAudioFile` object for the given URL or `nil` on failure
/// - note: Does not read audio properties or metadata
/// - parameter url: The URL
/// - parameter mimeTypeHint: A MIME type hint for `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioFile` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url
                        mimeTypeHint:(nullable NSString *)mimeTypeHint
                               error:(NSError **)error;
/// Returns an initialized `SFBAudioFile` object for the given URL or `nil` on failure
/// - note: Does not read audio properties or metadata
/// - parameter url: The URL
/// - parameter detectContentType: Whether to attempt to determine the content type of `url`
/// - parameter mimeTypeHint: A MIME type hint for `url`
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioFile` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url
                   detectContentType:(BOOL)detectContentType
                        mimeTypeHint:(nullable NSString *)mimeTypeHint
                               error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized `SFBAudioFile` object for the specified URL
/// - note: Does not read audio properties or metadata
/// - parameter url: The URL
/// - parameter formatName: The name of the format to use
- (nullable instancetype)initWithURL:(NSURL *)url
                          formatName:(SFBAudioFileFormatName)formatName
      NS_SWIFT_UNAVAILABLE("Use -initWithURL:formatName:error: instead");
/// Returns an initialized `SFBAudioFile` object for the specified URL
/// - note: Does not read audio properties or metadata
/// - parameter url: The URL
/// - parameter formatName: The name of the format to use
/// - parameter error: An optional pointer to an `NSError` object to receive error information
- (nullable instancetype)initWithURL:(NSURL *)url
                          formatName:(SFBAudioFileFormatName)formatName
                               error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// The URL of the file
@property(nonatomic, readonly) NSURL *url;

/// The file's audio properties
@property(nonatomic, readonly) SFBAudioProperties *properties;

/// The file's audio metadata
@property(nonatomic) SFBAudioMetadata *metadata;

/// Reads audio properties and metadata
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if successful, `NO` otherwise
- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error NS_SWIFT_NAME(readPropertiesAndMetadata());

/// Writes metadata
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: `YES` if successful, `NO` otherwise
- (BOOL)writeMetadataReturningError:(NSError **)error NS_SWIFT_NAME(writeMetadata());

@end

#pragma mark - Error Information

/// The `NSErrorDomain` used by `SFBAudioFile` and subclasses
extern NSErrorDomain const SFBAudioFileErrorDomain NS_SWIFT_NAME(AudioFile.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioFile`
typedef NS_ERROR_ENUM(SFBAudioFileErrorDomain, SFBAudioFileErrorCode){
    /// Internal or unspecified error
    SFBAudioFileErrorCodeInternalError = 0,
    /// Unknown format name
    SFBAudioFileErrorCodeUnknownFormatName = 1,
    /// Input/output error
    SFBAudioFileErrorCodeInputOutput = 2,
    /// Invalid, unknown, or unsupported format
    SFBAudioFileErrorCodeInvalidFormat = 3,
} NS_SWIFT_NAME(AudioFile.Error);

NS_ASSUME_NONNULL_END
