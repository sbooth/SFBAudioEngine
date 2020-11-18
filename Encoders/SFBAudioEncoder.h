/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBPCMEncoding.h>

NS_ASSUME_NONNULL_BEGIN

/// An encoder consuming PCM audio
NS_SWIFT_NAME(AudioEncoder) @interface SFBAudioEncoder : NSObject <SFBPCMEncoding>

#pragma mark - File Format Support

/// Returns a set containing the supported path extensions
@property (class, nonatomic, readonly) NSSet<NSString *> *supportedPathExtensions;

/*!@brief Returns a set containing the supported MIME types */
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

- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource NS_SWIFT_UNAVAILABLE("Use -initWithOutputSource:error: instead");
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource error:(NSError **)error;
- (nullable instancetype)initWithOutputSource:(SFBOutputSource *)outputSource mimeType:(nullable NSString *)mimeType error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (BOOL)openReturningError:(NSError **)error NS_REQUIRES_SUPER;
- (BOOL)closeReturningError:(NSError **)error NS_REQUIRES_SUPER;

@end

#pragma mark - Error Information

/// The \c NSErrorDomain used by \c SFBAudioEncoder and subclasses
extern NSErrorDomain const SFBAudioEncoderErrorDomain NS_SWIFT_NAME(AudioEncoder.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBAudioEncoder
typedef NS_ERROR_ENUM(SFBAudioEncoderErrorDomain, SFBAudioEncoderErrorCode) {
	/// File not found
	SFBAudioEncoderErrorCodeFileNotFound	= 0,
	/// Input/output error
	SFBAudioEncoderErrorCodeInputOutput		= 1,
	/// Invalid desired format
	SFBAudioEncoderErrorCodeInvalidFormat	= 2,
	/// Internal encoder error
	SFBAudioEncoderErrorCodeInternalError	= 3
} NS_SWIFT_NAME(AudioEncoder.ErrorCode);

#pragma mark - Encoder Settings

// Encoder settings dictionary keys
/// The FLAC compression level (\c NSNumber from 1 (lowest) to 8 (highest))
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACCompressionLevel;
/// Set to nonzero to verify FLAC encoding (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACVerifyEncoding;
/// The APE compression level (\c NSNumber)
extern SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyAPECompressionLevel;

/// Constants for APE compression levels
typedef NS_ENUM(int, SFBAudioEncoderAPECompressionLevel) {
	/// Fast compression
	SFBAudioEncoderAPECompressionLevelFast			= 1000,
	/// Normal compression
	SFBAudioEncoderAPECompressionLevelNormal		= 2000,
	/// High compression
	SFBAudioEncoderAPECompressionLevelHigh			= 3000,
	/// Extra high compression
	SFBAudioEncoderAPECompressionLevelExtraHigh		= 4000,
	/// Insane compression
	SFBAudioEncoderAPECompressionLevelInsane		= 5000
} NS_SWIFT_NAME(AudioEncoder.APECompressionLevel);

NS_ASSUME_NONNULL_END
