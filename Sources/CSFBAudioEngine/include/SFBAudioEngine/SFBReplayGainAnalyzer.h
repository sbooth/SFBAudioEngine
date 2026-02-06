//
// SPDX-FileCopyrightText: 2011 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Replay gain adjustment and peak sample amplitude
NS_SWIFT_NAME(ReplayGain)
@interface SFBReplayGain : NSObject
/// The replay gain adjustment in dB
@property(nonatomic, readonly) float gain;
/// The peak sample amplitude normalized to [-1, 1)
@property(nonatomic, readonly) float peak;
@end

/// Album replay gain information
NS_SWIFT_NAME(AlbumReplayGain)
@interface SFBAlbumReplayGain : NSObject
/// The album replay gain information
@property(nonatomic, nonnull, readonly) SFBReplayGain *replayGain;
/// The track replay gain information
@property(nonatomic, nonnull, readonly) NSDictionary<NSURL *, SFBReplayGain *> *trackReplayGain;
@end

/// A class that calculates revised replay gain using ITU BS.1770 loudness measurements with a reference level of -18
/// LUFS
/// - seealso: https://wiki.hydrogenaudio.org/index.php?title=Revised_ReplayGain_specification
NS_SWIFT_NAME(ReplayGainAnalyzer)
@interface SFBReplayGainAnalyzer : NSObject

/// Calculates replay gain for a single track
/// - parameter url: The URL to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: The replay gain information or `nil` on error
+ (nullable SFBReplayGain *)analyzeTrack:(NSURL *)url error:(NSError **)error;

/// Calculates replay gain for an album
/// - parameter urls: The URLs to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: The replay gain information or `nil` on error
+ (nullable SFBAlbumReplayGain *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error;

/// Calculates replay gain for a single track
/// - parameter url: The URL to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: The replay gain information or `nil` on error
- (nullable SFBReplayGain *)analyzeTrack:(NSURL *)url error:(NSError **)error;

/// Calculates replay gain for an album
/// - parameter urls: The URLs to analyze
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: The replay gain information or `nil` on error
- (nullable SFBAlbumReplayGain *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error;

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
