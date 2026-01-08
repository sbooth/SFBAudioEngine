//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <SFBAudioEngine/SFBPCMDecoding.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio decoder supporting a repeating audio region
NS_SWIFT_NAME(AudioRegionDecoder) @interface SFBAudioRegionDecoder : NSObject <SFBPCMDecoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized `SFBAudioRegionDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url startingFrame:(AVAudioFramePosition)startingFrame error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter repeatCount: The number of times to repeat the audio region. A value of zero results in the region playing once and a value of -1 results in a continuous loop.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url repeatCount:(NSInteger)repeatCount error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given URL or `nil` on failure
/// - parameter url: The URL
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter repeatCount: The number of times to repeat the audio region. A value of zero results in the region playing once and a value of -1 results in a continuous loop.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified URL, or `nil` on failure
- (nullable instancetype)initWithURL:(NSURL *)url startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

/// Returns an initialized `SFBAudioRegionDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource startingFrame:(AVAudioFramePosition)startingFrame error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter repeatCount: The number of times to repeat the audio region. A value of -1 results in a continuous loop.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource repeatCount:(NSInteger)repeatCount error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given input source or `nil` on failure
/// - parameter inputSource: The input source
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter repeatCount: The number of times to repeat the audio region. A value of -1 results in a continuous loop.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified input source, or `nil` on failure
- (nullable instancetype)initWithInputSource:(SFBInputSource *)inputSource startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error;

/// Returns an initialized `SFBAudioRegionDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startingFrame:(AVAudioFramePosition)startingFrame error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter repeatCount: The number of times to repeat the audio region. A value of -1 results in a continuous loop.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder repeatCount:(NSInteger)repeatCount error:(NSError **)error;
/// Returns an initialized `SFBAudioRegionDecoder` object for the given decoder or `nil` on failure
/// - parameter decoder: The decoder
/// - parameter startingFrame: The starting frame position of the audio region
/// - parameter frameLength: The frame length of the audio region. A value of -1 results in playing until the end of the audio.
/// - parameter repeatCount: The number of times to repeat the audio region. A value of -1 results in a continuous loop.
/// - parameter error: An optional pointer to an `NSError` object to receive error information
/// - returns: An initialized `SFBAudioRegionDecoder` object for the specified decoder, or `nil` on failure
- (nullable instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/// The starting frame position of the audio region
@property (nonatomic, readonly) AVAudioFramePosition regionStartingFrame;

/// The frame length of the audio region
@property (nonatomic, readonly) AVAudioFramePosition regionFrameLength;

/// The current frame offset within the audio region relative to the region's starting frame
@property (nonatomic, readonly) AVAudioFramePosition regionFrameOffset;

/// The number of times the audio region will be repeated
@property (nonatomic, readonly) NSInteger repeatCount;

/// The number of completed loops
@property (nonatomic, readonly) NSInteger completedLoops;

@end

NS_ASSUME_NONNULL_END
