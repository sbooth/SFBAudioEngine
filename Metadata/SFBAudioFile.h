//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

#import <SFBAudioEngine/SFBAudioProperties.h>
#import <SFBAudioEngine/SFBAudioMetadata.h>

NS_ASSUME_NONNULL_BEGIN

/// Constant type for file format names
typedef NSString * SFBAudioFileFormatName NS_TYPED_ENUM NS_SWIFT_NAME(AudioFile.FormatName);

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

/// Returns an initialized \c SFBAudioFile object for the given URL or \c nil on failure
/// @note Does not read audio properties or metadata
/// @param url The URL
- (nullable instancetype)initWithURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use -initWithURL:error: instead");
/// Returns an initialized \c SFBAudioFile object for the given URL or \c nil on failure
/// @note Does not read audio properties or metadata
/// @param url The URL
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioFile object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
/// Returns an initialized \c SFBAudioFile object for the given URL or \c nil on failure
/// @param url The URL
/// @param mimeType The MIME type of \c url or \c nil
/// @param error An optional pointer to a \c NSError to receive error information
/// @return An initialized \c SFBAudioFile object for the specified URL, or \c nil on failure
- (nullable instancetype)initWithURL:(NSURL *)url mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// Returns an initialized \c SFBAudioFile object for the specified URL
/// @note Does not read audio properties or metadata
/// @param url The URL
/// @param formatName The name of the format to use
- (nullable instancetype)initWithURL:(NSURL *)url formatName:(SFBAudioFileFormatName)formatName NS_SWIFT_UNAVAILABLE("Use -initWithURL:formatName:error: instead");
/// Returns an initialized \c SFBAudioFile object for the specified URL
/// @note Does not read audio properties or metadata
/// @param url The URL
/// @param formatName The name of the format to use
/// @param error An optional pointer to a \c NSError to receive error information
- (nullable instancetype)initWithURL:(NSURL *)url formatName:(SFBAudioFileFormatName)formatName error:(NSError **)error NS_DESIGNATED_INITIALIZER;

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
	/// Internal or unspecified error
	SFBAudioFileErrorCodeInternalError		= 0,
	/// Unknown format name
	SFBAudioFileErrorCodeUnknownFormatName	= 1,
	/// Input/output error
	SFBAudioFileErrorCodeInputOutput		= 2,
	/// Invalid, unknown, or unsupported format
	SFBAudioFileErrorCodeInvalidFormat		= 3
} NS_SWIFT_NAME(AudioFile.ErrorCode);

NS_ASSUME_NONNULL_END
