//
// SPDX-FileCopyrightText: 2011 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// A key in a replay gain dictionary
typedef NSString *SFBReplayGainAnalyzerKey NS_TYPED_ENUM NS_SWIFT_NAME(ReplayGainAnalyzer.Key);

// Replay gain dictionary keys
/// The gain in dB (`NSNumber`)
extern SFBReplayGainAnalyzerKey const SFBReplayGainAnalyzerKeyGain;
/// The peak sample value normalized to [-1, 1) (`NSNumber`)
extern SFBReplayGainAnalyzerKey const SFBReplayGainAnalyzerKeyPeak;

/// A class that calculates revised replay gain
/// - seealso: https://wiki.hydrogenaudio.org/index.php?title=Revised_ReplayGain_specification
NS_SWIFT_NAME(ReplayGainAnalyzer)
@interface SFBReplayGainAnalyzer : NSObject

/// Calculates replay gain for a single track
/// - parameter url: The URL to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: A dictionary containing the track gain in dB (`SFBReplayGainAnalyzerKeyGain`) and peak sample value
/// normalized to [-1, 1) (`SFBReplayGainAnalyzerKeyPeak`), or `nil` on error
+ (nullable NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)analyzeTrack:(NSURL *)url
                                                                        error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Calculates replay gain for an album
///
/// The returned dictionary will contain the album gain and peak sample and the track gains and peak samples keyed by
/// URL
/// - parameter urls: The URLs to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: A dictionary of gain and peak information, or `nil` on error
+ (nullable NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Calculates replay gain for a single track
/// - parameter url: The URL to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: A dictionary containing the track gain in dB (`SFBReplayGainAnalyzerKeyGain`) and peak sample value
/// normalized to [-1, 1) (`SFBReplayGainAnalyzerKeyPeak`), or `nil` on error
- (nullable NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)analyzeTrack:(NSURL *)url
                                                                        error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Calculates replay gain for an album
///
/// The returned dictionary will contain the album gain and peak sample, and the track gains and peak samples keyed by
/// URL
/// - parameter urls: The URLs to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: A dictionary of gain and peak information, or `nil` on error
- (nullable NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error NS_REFINED_FOR_SWIFT;

@end

/// The `NSErrorDomain` used by `SFBReplayGainAnalyzer`
extern NSErrorDomain const SFBReplayGainAnalyzerErrorDomain NS_SWIFT_NAME(ReplayGainAnalyzer.ErrorDomain);

/// Possible `NSError` error codes used by `SFBReplayGainAnalyzer`
typedef NS_ERROR_ENUM(SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCode){
    /// File format not supported
    SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported = 0,
    /// Insufficient samples in file for analysis
    SFBReplayGainAnalyzerErrorCodeInsufficientSamples = 1,
} NS_SWIFT_NAME(ReplayGainAnalyzer.Error);

NS_ASSUME_NONNULL_END
