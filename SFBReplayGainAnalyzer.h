/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*! @brief The \c NSErrorDomain used by \c SFBReplayGainAnalyzer and subclasses */
extern NSErrorDomain const SFBReplayGainAnalyzerErrorDomain;


/*! @brief Possible \c NSError  error codes used by \c SFBReplayGainAnalyzer */
typedef NS_ENUM(NSUInteger, SFBReplayGainAnalyzerErrorCode) {
	SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported		= 0,	/*!< File format not supported */
};


/*! @name Replay gain  dictionary keys */
//@{
extern NSString * const SFBReplayGainAnalyzerGainKey;					/*!< @brief The gain in dB (\c NSNumber) */
extern NSString * const SFBReplayGainAnalyzerPeakKey;					/*!< @brief The peak value normalized to [-1, 1) (\c NSNumber) */
//@}


/*!
 * @brief A class that calculates replay gain
 * @see http://wiki.hydrogenaudio.org/index.php?title=ReplayGain_specification
 */
@interface SFBReplayGainAnalyzer : NSObject

/*! @brief The reference loudness in dB SPL, defined as 89.0 dB */
@property (class, nonatomic, readonly) float referenceLoudness;


/*!
 * @brief Analyze the given album's replay gain
 *
 * @discussion The returned dictionary will contain the entries returned by \c -albumGainAndPeakSample and the
 * results of \c -trackGainAndPeakSample keyed by URL
 * @param urls The URLs to analyze
 * @param error An optional pointer to an \c NSError  to receive error information
 * @return A dictionary of gain and peak information, or \c nil on error
*/
+ (nullable NSDictionary<NSString *, id> *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError * _Nullable *)error;

/*!
 * @brief Analyze the given URL's replay gain
 *
 * If the URL's sample rate is not natively supported, the replay gain adjustment will be calculated using audio
 * resampled to an even multiple sample rate
 * @param url The URL to analyze
 * @param error An optional pointer to an \c NSError  to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)analyzeTrack:(NSURL *)url error:(NSError * _Nullable *)error;


/*! @brief The track gain in dB (\c SFBReplayGainAnalyzerGainKey) and peak sample value normalized to [-1, 1) (\c SFBReplayGainAnalyzerPeakKey), or \c nil on error */
@property (nonatomic, nullable, readonly) NSDictionary<NSString *, NSNumber *>* trackGainAndPeakSample;

/*! @brief The album gain in dB (\c SFBReplayGainAnalyzerGainKey) and peak sample value normalized to [-1, 1) (\c SFBReplayGainAnalyzerPeakKey), or \c nil on error */
@property (nonatomic, nullable, readonly) NSDictionary<NSString *, NSNumber *>* albumGainAndPeakSample;

@end

NS_ASSUME_NONNULL_END
