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

#pragma mark - Legacy Error Domain Compatibility

/// Legacy error domain for `SFBAudioDecoder`
#define SFBAudioDecoderErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBAudioDecoder`
#define SFBAudioDecoderErrorCodeUnknownDecoder			SFBAudioEngineErrorCodeUnknownDecoder
#define SFBAudioDecoderErrorCodeInvalidFormat			SFBAudioEngineErrorCodeInvalidFormat
#define SFBAudioDecoderErrorCodeUnsupportedFormat		SFBAudioEngineErrorCodeUnsupportedFormat
#define SFBAudioDecoderErrorCodeInternalError			SFBAudioEngineErrorCodeInternalError
#define SFBAudioDecoderErrorCodeDecodingError			SFBAudioEngineErrorCodeDecodingError
#define SFBAudioDecoderErrorCodeSeekError				SFBAudioEngineErrorCodeSeekError

/// Legacy error domain for `SFBDSDDecoder`
#define SFBDSDDecoderErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBDSDDecoder`
#define SFBDSDDecoderErrorCodeUnknownDecoder			SFBAudioEngineErrorCodeUnknownDecoder
#define SFBDSDDecoderErrorCodeInvalidFormat				SFBAudioEngineErrorCodeInvalidFormat
#define SFBDSDDecoderErrorCodeUnsupportedFormat			SFBAudioEngineErrorCodeUnsupportedFormat
#define SFBDSDDecoderErrorCodeInternalError				SFBAudioEngineErrorCodeInternalError
#define SFBDSDDecoderErrorCodeDecodingError				SFBAudioEngineErrorCodeDecodingError
#define SFBDSDDecoderErrorCodeSeekError					SFBAudioEngineErrorCodeSeekError

/// Legacy error domain for `SFBAudioEncoder`
#define SFBAudioEncoderErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBAudioEncoder`
#define SFBAudioEncoderErrorCodeUnknownEncoder			SFBAudioEngineErrorCodeUnknownEncoder
#define SFBAudioEncoderErrorCodeInvalidFormat			SFBAudioEngineErrorCodeInvalidFormat
#define SFBAudioEncoderErrorCodeInternalError			SFBAudioEngineErrorCodeInternalError

/// Legacy error domain for `SFBAudioFile`
#define SFBAudioFileErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBAudioFile`
#define SFBAudioFileErrorCodeInternalError				SFBAudioEngineErrorCodeInternalError
#define SFBAudioFileErrorCodeUnknownFormatName			SFBAudioEngineErrorCodeUnknownFormatName
#define SFBAudioFileErrorCodeInputOutput				SFBAudioEngineErrorCodeInputOutput
#define SFBAudioFileErrorCodeInvalidFormat				SFBAudioEngineErrorCodeInvalidFormat

/// Legacy error domain for `SFBAudioPlayer`
#define SFBAudioPlayerErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBAudioPlayer`
#define SFBAudioPlayerErrorCodeInternalError			SFBAudioEngineErrorCodeInternalError
#define SFBAudioPlayerErrorCodeFormatNotSupported		SFBAudioEngineErrorCodeFormatNotSupported

/// Legacy error domain for `SFBInputSource`
#define SFBInputSourceErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBInputSource`
#define SFBInputSourceErrorCodeFileNotFound				SFBAudioEngineErrorCodeFileNotFound
#define SFBInputSourceErrorCodeInputOutput				SFBAudioEngineErrorCodeInputOutput
#define SFBInputSourceErrorCodeNotSeekable				SFBAudioEngineErrorCodeNotSeekable

/// Legacy error domain for `SFBOutputSource`
#define SFBOutputSourceErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBOutputSource`
#define SFBOutputSourceErrorCodeFileNotFound			SFBAudioEngineErrorCodeFileNotFound
#define SFBOutputSourceErrorCodeInputOutput				SFBAudioEngineErrorCodeInputOutput

/// Legacy error domain for `SFBReplayGainAnalyzer`
#define SFBReplayGainAnalyzerErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBReplayGainAnalyzer`
#define SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported	SFBAudioEngineErrorCodeFormatNotSupported
#define SFBReplayGainAnalyzerErrorCodeInsufficientSamples		SFBAudioEngineErrorCodeInsufficientSamples

/// Legacy error domain for `SFBAudioConverter`
#define SFBAudioConverterErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBAudioConverter`
#define SFBAudioConverterErrorCodeFormatNotSupported		SFBAudioEngineErrorCodeFormatNotSupported

/// Legacy error domain for `SFBAudioExporter`
#define SFBAudioExporterErrorDomain SFBAudioEngineErrorDomain

/// Legacy error codes for `SFBAudioExporter`
#define SFBAudioExporterErrorCodeFileFormatNotSupported		SFBAudioEngineErrorCodeFormatNotSupported

NS_ASSUME_NONNULL_END
