//
// Copyright (c) 2011-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// A key in a replay gain dictionary
typedef NSString * SFBReplayGainAnalyzerKey NS_TYPED_ENUM NS_SWIFT_NAME(ReplayGainAnalyzer.Key);

// Replay gain dictionary keys
/// The gain in dB (`NSNumber`)
extern SFBReplayGainAnalyzerKey const SFBReplayGainAnalyzerGainKey;
/// The peak value normalized to [-1, 1) (`NSNumber`)
extern SFBReplayGainAnalyzerKey const SFBReplayGainAnalyzerPeakKey;


/// A class that calculates replay gain
/// - seealso: http://wiki.hydrogenaudio.org/index.php?title=ReplayGain_specification
NS_SWIFT_NAME(ReplayGainAnalyzer) @interface SFBReplayGainAnalyzer : NSObject

/// The reference loudness in dB SPL, defined as 89.0 dB
@property (class, nonatomic, readonly) float referenceLoudness;


/// Analyze the given album's replay gain
///
/// The returned dictionary will contain the entries returned by ``-albumGainAndPeakSample`` and the
/// results of ``-trackGainAndPeakSample`` keyed by URL
/// - parameter urls: The URLs to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: A dictionary of gain and peak information, or `nil` on error
+ (nullable NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Analyze the given URL's replay gain
///
/// If the URL's sample rate is not natively supported, the replay gain adjustment will be calculated using audio
/// resampled to an even multiple sample rate
/// - parameter url: The URL to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: A dictionary containing the track gain in dB (`SFBReplayGainAnalyzerGainKey`) and peak sample value normalized to [-1, 1) (`SFBReplayGainAnalyzerPeakKey`), or `nil` on error
- (nullable NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)analyzeTrack:(NSURL *)url error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the album gain in dB (`SFBReplayGainAnalyzerGainKey`) and peak sample value normalized to [-1, 1) (`SFBReplayGainAnalyzerPeakKey`), or `nil` on error
- (nullable NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)albumGainAndPeakSampleReturningError:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

/// The `NSErrorDomain` used by `SFBReplayGainAnalyzer`
#define SFBReplayGainAnalyzerErrorDomain SFBAudioEngineErrorDomain

/// Possible `NSError` error codes used by `SFBReplayGainAnalyzer`
#define SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported	SFBAudioEngineErrorCodeFormatNotSupported
#define SFBReplayGainAnalyzerErrorCodeInsufficientSamples		SFBAudioEngineErrorCodeInsufficientSamples

NS_ASSUME_NONNULL_END
