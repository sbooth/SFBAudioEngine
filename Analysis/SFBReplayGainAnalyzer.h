/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NSString * SFBReplayGainAnalyzerKey NS_TYPED_ENUM NS_SWIFT_NAME(ReplayGainAnalyzer.Key);

// Replay gain dictionary keys
/// The gain in dB (\c NSNumber)
extern SFBReplayGainAnalyzerKey const SFBReplayGainAnalyzerGainKey;
/// The peak value normalized to [-1, 1) (\c NSNumber)
extern SFBReplayGainAnalyzerKey const SFBReplayGainAnalyzerPeakKey;


/// A class that calculates replay gain
/// @see http://wiki.hydrogenaudio.org/index.php?title=ReplayGain_specification
NS_SWIFT_NAME(ReplayGainAnalyzer) @interface SFBReplayGainAnalyzer : NSObject

/// The reference loudness in dB SPL, defined as 89.0 dB
@property (class, nonatomic, readonly) float referenceLoudness;


/// Analyze the given album's replay gain
///
/// The returned dictionary will contain the entries returned by \c -albumGainAndPeakSample and the
/// results of \c -trackGainAndPeakSample keyed by URL
/// @param urls The URLs to analyze
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return A dictionary of gain and peak information, or \c nil on error
+ (nullable NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Analyze the given URL's replay gain
///
/// If the URL's sample rate is not natively supported, the replay gain adjustment will be calculated using audio
/// resampled to an even multiple sample rate
/// @param url The URL to analyze
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return A dictionary containing the track gain in dB (\c SFBReplayGainAnalyzerGainKey) and peak sample value normalized to [-1, 1) (\c SFBReplayGainAnalyzerPeakKey), or \c nil on error
- (nullable NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)analyzeTrack:(NSURL *)url error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the album gain in dB (\c SFBReplayGainAnalyzerGainKey) and peak sample value normalized to [-1, 1) (\c SFBReplayGainAnalyzerPeakKey), or \c nil on error
- (nullable NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)albumGainAndPeakSampleReturningError:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

/// The \c NSErrorDomain used by \c SFBReplayGainAnalyzer
extern NSErrorDomain const SFBReplayGainAnalyzerErrorDomain NS_SWIFT_NAME(ReplayGainAnalyzer.ErrorDomain);

/// Possible \c NSError error codes used by \c SFBReplayGainAnalyzer
typedef NS_ERROR_ENUM(SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCode) {
	/// File format not supported
	SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported		= 0,
	/// Insufficient samples in file for analysis
	SFBReplayGainAnalyzerErrorCodeInsufficientSamples			= 1,
} NS_SWIFT_NAME(ReplayGainAnalyzer.ErrorCode);

NS_ASSUME_NONNULL_END
