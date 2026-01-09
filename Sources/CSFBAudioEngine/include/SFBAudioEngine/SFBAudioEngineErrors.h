//
// Copyright (c) 2024-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// The unified `NSErrorDomain` used by `SFBAudioEngine`
extern NSErrorDomain const SFBAudioEngineErrorDomain NS_SWIFT_NAME(AudioEngine.ErrorDomain);

/// Possible `NSError` error codes used by `SFBAudioEngine`
typedef NS_ERROR_ENUM(SFBAudioEngineErrorDomain, SFBAudioEngineErrorCode) {
	// MARK: General Errors
	/// Internal or unspecified error
	SFBAudioEngineErrorCodeInternalError				= 0,
	/// File not found
	SFBAudioEngineErrorCodeFileNotFound					= 1,
	/// Input/output error
	SFBAudioEngineErrorCodeInputOutput					= 2,
	
	// MARK: Format Errors
	/// Invalid or unknown format
	SFBAudioEngineErrorCodeInvalidFormat				= 10,
	/// Format is recognized but not supported by the decoder/encoder
	SFBAudioEngineErrorCodeUnsupportedFormat			= 11,
	/// Format is not supported for the current operation (conversion, export, analysis)
	SFBAudioEngineErrorCodeFormatNotSupported			= 12,
	
	// MARK: Decoder Errors
	/// Unknown decoder name
	SFBAudioEngineErrorCodeUnknownDecoder				= 20,
	/// Decoding error
	SFBAudioEngineErrorCodeDecodingError				= 21,
	/// Seek error
	SFBAudioEngineErrorCodeSeekError					= 22,
	
	// MARK: Encoder Errors
	/// Unknown encoder name
	SFBAudioEngineErrorCodeUnknownEncoder				= 30,
	
	// MARK: File Errors
	/// Unknown format name
	SFBAudioEngineErrorCodeUnknownFormatName			= 40,
	
	// MARK: Input/Output Source Errors
	/// Input not seekable
	SFBAudioEngineErrorCodeNotSeekable					= 50,
	
	// MARK: Replay Gain Analyzer Errors
	/// Insufficient samples in file for analysis
	SFBAudioEngineErrorCodeInsufficientSamples			= 60,
} NS_SWIFT_NAME(AudioEngine.ErrorCode);

NS_ASSUME_NONNULL_END
